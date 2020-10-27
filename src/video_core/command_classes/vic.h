// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "common/bit_field.h"
#include "common/common_types.h"

struct SwsContext;

namespace Tegra {
class GPU;
class Nvdec;

struct PlaneOffsets {
    u32 luma_offset{};
    u32 chroma_u_offset{};
    u32 chroma_v_offset{};
};

struct VicRegisters {
    INSERT_PADDING_WORDS(64);
    u32 nop{};
    INSERT_PADDING_WORDS(15);
    u32 pm_trigger{};
    INSERT_PADDING_WORDS(47);
    u32 set_application_id{};
    u32 set_watchdog_timer{};
    INSERT_PADDING_WORDS(17);
    u32 context_save_area{};
    u32 context_switch{};
    INSERT_PADDING_WORDS(43);
    u32 execute{};
    INSERT_PADDING_WORDS(63);
    std::array<std::array<PlaneOffsets, 8>, 8> surfacex_slots{};
    u32 picture_index{};
    u32 control_params{};
    u32 config_struct_offset{};
    u32 filter_struct_offset{};
    u32 palette_offset{};
    u32 hist_offset{};
    u32 context_id{};
    u32 fce_ucode_size{};
    PlaneOffsets output_surface{};
    u32 fce_ucode_offset{};
    INSERT_PADDING_WORDS(4);
    std::array<u32, 8> slot_context_id{};
    INSERT_PADDING_WORDS(16);
};
static_assert(sizeof(VicRegisters) == 0x7A0, "VicRegisters is an invalid size");

class Vic {
public:
    enum class Method : u32 {
        Execute = 0xc0,
        SetControlParams = 0x1c1,
        SetConfigStructOffset = 0x1c2,
        SetOutputSurfaceLumaOffset = 0x1c8,
        SetOutputSurfaceChromaUOffset = 0x1c9,
        SetOutputSurfaceChromaVOffset = 0x1ca
    };

    explicit Vic(GPU& gpu, std::shared_ptr<Tegra::Nvdec> nvdec_processor);
    ~Vic();

    /// Write to the device state.
    void ProcessMethod(Vic::Method method, const std::vector<u32>& arguments);

private:
    void Execute();

    void VicStateWrite(u32 offset, u32 arguments);
    VicRegisters vic_state{};

    enum class VideoPixelFormat : u64_le {
        RGBA8 = 0x1f,
        BGRA8 = 0x20,
        Yuv420 = 0x44,
    };

    union VicConfig {
        u64_le raw{};
        BitField<0, 7, u64_le> pixel_format;
        BitField<7, 2, u64_le> chroma_loc_horiz;
        BitField<9, 2, u64_le> chroma_loc_vert;
        BitField<11, 4, u64_le> block_linear_kind;
        BitField<15, 4, u64_le> block_linear_height_log2;
        BitField<19, 3, u64_le> reserved0;
        BitField<22, 10, u64_le> reserved1;
        BitField<32, 14, u64_le> surface_width_minus1;
        BitField<46, 14, u64_le> surface_height_minus1;
    };

    GPU& gpu;
    std::shared_ptr<Tegra::Nvdec> nvdec_processor;

    GPUVAddr config_struct_address{};
    GPUVAddr output_surface_luma_address{};
    GPUVAddr output_surface_chroma_u_address{};
    GPUVAddr output_surface_chroma_v_address{};

    SwsContext* scaler_ctx{};
    s32 scaler_width{};
    s32 scaler_height{};
};

} // namespace Tegra
