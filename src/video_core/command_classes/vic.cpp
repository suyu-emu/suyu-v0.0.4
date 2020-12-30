// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include "common/assert.h"
#include "video_core/command_classes/nvdec.h"
#include "video_core/command_classes/vic.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/textures/decoders.h"

extern "C" {
#include <libswscale/swscale.h>
}

namespace Tegra {

Vic::Vic(GPU& gpu_, std::shared_ptr<Nvdec> nvdec_processor_)
    : gpu(gpu_), nvdec_processor(std::move(nvdec_processor_)) {}
Vic::~Vic() = default;

void Vic::VicStateWrite(u32 offset, u32 arguments) {
    u8* const state_offset = reinterpret_cast<u8*>(&vic_state) + offset * sizeof(u32);
    std::memcpy(state_offset, &arguments, sizeof(u32));
}

void Vic::ProcessMethod(Method method, const std::vector<u32>& arguments) {
    LOG_DEBUG(HW_GPU, "Vic method 0x{:X}", method);
    VicStateWrite(static_cast<u32>(method), arguments[0]);
    const u64 arg = static_cast<u64>(arguments[0]) << 8;
    switch (method) {
    case Method::Execute:
        Execute();
        break;
    case Method::SetConfigStructOffset:
        config_struct_address = arg;
        break;
    case Method::SetOutputSurfaceLumaOffset:
        output_surface_luma_address = arg;
        break;
    case Method::SetOutputSurfaceChromaUOffset:
        output_surface_chroma_u_address = arg;
        break;
    case Method::SetOutputSurfaceChromaVOffset:
        output_surface_chroma_v_address = arg;
        break;
    default:
        break;
    }
}

void Vic::Execute() {
    if (output_surface_luma_address == 0) {
        LOG_ERROR(Service_NVDRV, "VIC Luma address not set. Recieved 0x{:X}",
                  vic_state.output_surface.luma_offset);
        return;
    }
    const VicConfig config{gpu.MemoryManager().Read<u64>(config_struct_address + 0x20)};
    const AVFramePtr frame_ptr = nvdec_processor->GetFrame();
    const auto* frame = frame_ptr.get();
    if (!frame || frame->width == 0 || frame->height == 0) {
        return;
    }
    const VideoPixelFormat pixel_format =
        static_cast<VideoPixelFormat>(config.pixel_format.Value());
    switch (pixel_format) {
    case VideoPixelFormat::BGRA8:
    case VideoPixelFormat::RGBA8: {
        LOG_TRACE(Service_NVDRV, "Writing RGB Frame");

        if (scaler_ctx == nullptr || frame->width != scaler_width ||
            frame->height != scaler_height) {
            const AVPixelFormat target_format =
                (pixel_format == VideoPixelFormat::RGBA8) ? AV_PIX_FMT_RGBA : AV_PIX_FMT_BGRA;

            sws_freeContext(scaler_ctx);
            scaler_ctx = nullptr;

            // FFmpeg returns all frames in YUV420, convert it into expected format
            scaler_ctx =
                sws_getContext(frame->width, frame->height, AV_PIX_FMT_YUV420P, frame->width,
                               frame->height, target_format, 0, nullptr, nullptr, nullptr);

            scaler_width = frame->width;
            scaler_height = frame->height;
        }
        // Get Converted frame
        const std::size_t linear_size = frame->width * frame->height * 4;

        using AVMallocPtr = std::unique_ptr<u8, decltype(&av_free)>;
        AVMallocPtr converted_frame_buffer{static_cast<u8*>(av_malloc(linear_size)), av_free};

        const int converted_stride{frame->width * 4};
        u8* const converted_frame_buf_addr{converted_frame_buffer.get()};

        sws_scale(scaler_ctx, frame->data, frame->linesize, 0, frame->height,
                  &converted_frame_buf_addr, &converted_stride);

        const u32 blk_kind = static_cast<u32>(config.block_linear_kind);
        if (blk_kind != 0) {
            // swizzle pitch linear to block linear
            const u32 block_height = static_cast<u32>(config.block_linear_height_log2);
            const auto size = Tegra::Texture::CalculateSize(true, 4, frame->width, frame->height, 1,
                                                            block_height, 0);
            std::vector<u8> swizzled_data(size);
            Tegra::Texture::SwizzleSubrect(frame->width, frame->height, frame->width * 4,
                                           frame->width, 4, swizzled_data.data(),
                                           converted_frame_buffer.get(), block_height, 0, 0);

            gpu.MemoryManager().WriteBlock(output_surface_luma_address, swizzled_data.data(), size);
            gpu.Maxwell3D().OnMemoryWrite();
        } else {
            // send pitch linear frame
            gpu.MemoryManager().WriteBlock(output_surface_luma_address, converted_frame_buf_addr,
                                           linear_size);
            gpu.Maxwell3D().OnMemoryWrite();
        }
        break;
    }
    case VideoPixelFormat::Yuv420: {
        LOG_TRACE(Service_NVDRV, "Writing YUV420 Frame");

        const std::size_t surface_width = config.surface_width_minus1 + 1;
        const std::size_t surface_height = config.surface_height_minus1 + 1;
        const std::size_t half_width = surface_width / 2;
        const std::size_t half_height = config.surface_height_minus1 / 2;
        const std::size_t aligned_width = (surface_width + 0xff) & ~0xff;

        const auto* luma_ptr = frame->data[0];
        const auto* chroma_b_ptr = frame->data[1];
        const auto* chroma_r_ptr = frame->data[2];
        const auto stride = frame->linesize[0];
        const auto half_stride = frame->linesize[1];

        std::vector<u8> luma_buffer(aligned_width * surface_height);
        std::vector<u8> chroma_buffer(aligned_width * half_height);

        // Populate luma buffer
        for (std::size_t y = 0; y < surface_height - 1; ++y) {
            std::size_t src = y * stride;
            std::size_t dst = y * aligned_width;

            std::size_t size = surface_width;

            for (std::size_t offset = 0; offset < size; ++offset) {
                luma_buffer[dst + offset] = luma_ptr[src + offset];
            }
        }
        gpu.MemoryManager().WriteBlock(output_surface_luma_address, luma_buffer.data(),
                                       luma_buffer.size());

        // Populate chroma buffer from both channels with interleaving.
        for (std::size_t y = 0; y < half_height; ++y) {
            std::size_t src = y * half_stride;
            std::size_t dst = y * aligned_width;

            for (std::size_t x = 0; x < half_width; ++x) {
                chroma_buffer[dst + x * 2] = chroma_b_ptr[src + x];
                chroma_buffer[dst + x * 2 + 1] = chroma_r_ptr[src + x];
            }
        }
        gpu.MemoryManager().WriteBlock(output_surface_chroma_u_address, chroma_buffer.data(),
                                       chroma_buffer.size());
        gpu.Maxwell3D().OnMemoryWrite();
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unknown video pixel format {}", config.pixel_format.Value());
        break;
    }
}

} // namespace Tegra
