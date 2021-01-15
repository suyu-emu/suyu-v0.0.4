// MIT License
//
// Copyright (c) Ryujinx Team and Contributors
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
// associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <bit>
#include "command_classes/host1x.h"
#include "command_classes/nvdec.h"
#include "command_classes/vic.h"
#include "video_core/cdma_pusher.h"
#include "video_core/command_classes/nvdec_common.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"

namespace Tegra {
CDmaPusher::CDmaPusher(GPU& gpu_)
    : gpu{gpu_}, nvdec_processor(std::make_shared<Nvdec>(gpu)),
      vic_processor(std::make_unique<Vic>(gpu, nvdec_processor)),
      host1x_processor(std::make_unique<Host1x>(gpu)),
      sync_manager(std::make_unique<SyncptIncrManager>(gpu)) {}

CDmaPusher::~CDmaPusher() = default;

void CDmaPusher::Push(ChCommandHeaderList&& entries) {
    cdma_queue.push(std::move(entries));
}

void CDmaPusher::DispatchCalls() {
    while (!cdma_queue.empty()) {
        Step();
    }
}

void CDmaPusher::Step() {
    const auto entries{cdma_queue.front()};
    cdma_queue.pop();

    std::vector<u32> values(entries.size());
    std::memcpy(values.data(), entries.data(), entries.size() * sizeof(u32));

    for (const u32 value : values) {
        if (mask != 0) {
            const auto lbs = static_cast<u32>(std::countr_zero(mask));
            mask &= ~(1U << lbs);
            ExecuteCommand(static_cast<u32>(offset + lbs), value);
            continue;
        } else if (count != 0) {
            --count;
            ExecuteCommand(static_cast<u32>(offset), value);
            if (incrementing) {
                ++offset;
            }
            continue;
        }
        const auto mode = static_cast<ChSubmissionMode>((value >> 28) & 0xf);
        switch (mode) {
        case ChSubmissionMode::SetClass: {
            mask = value & 0x3f;
            offset = (value >> 16) & 0xfff;
            current_class = static_cast<ChClassId>((value >> 6) & 0x3ff);
            break;
        }
        case ChSubmissionMode::Incrementing:
        case ChSubmissionMode::NonIncrementing:
            count = value & 0xffff;
            offset = (value >> 16) & 0xfff;
            incrementing = mode == ChSubmissionMode::Incrementing;
            break;
        case ChSubmissionMode::Mask:
            mask = value & 0xffff;
            offset = (value >> 16) & 0xfff;
            break;
        case ChSubmissionMode::Immediate: {
            const u32 data = value & 0xfff;
            offset = (value >> 16) & 0xfff;
            ExecuteCommand(static_cast<u32>(offset), data);
            break;
        }
        default:
            UNIMPLEMENTED_MSG("ChSubmission mode {} is not implemented!", static_cast<u32>(mode));
            break;
        }
    }
}

void CDmaPusher::ExecuteCommand(u32 state_offset, u32 data) {
    switch (current_class) {
    case ChClassId::NvDec:
        ThiStateWrite(nvdec_thi_state, state_offset, {data});
        switch (static_cast<ThiMethod>(state_offset)) {
        case ThiMethod::IncSyncpt: {
            LOG_DEBUG(Service_NVDRV, "NVDEC Class IncSyncpt Method");
            const auto syncpoint_id = static_cast<u32>(data & 0xFF);
            const auto cond = static_cast<u32>((data >> 8) & 0xFF);
            if (cond == 0) {
                sync_manager->Increment(syncpoint_id);
            } else {
                sync_manager->SignalDone(
                    sync_manager->IncrementWhenDone(static_cast<u32>(current_class), syncpoint_id));
            }
            break;
        }
        case ThiMethod::SetMethod1:
            LOG_DEBUG(Service_NVDRV, "NVDEC method 0x{:X}",
                      static_cast<u32>(nvdec_thi_state.method_0));
            nvdec_processor->ProcessMethod(static_cast<Nvdec::Method>(nvdec_thi_state.method_0),
                                           {data});
            break;
        default:
            break;
        }
        break;
    case ChClassId::GraphicsVic:
        ThiStateWrite(vic_thi_state, static_cast<u32>(state_offset), {data});
        switch (static_cast<ThiMethod>(state_offset)) {
        case ThiMethod::IncSyncpt: {
            LOG_DEBUG(Service_NVDRV, "VIC Class IncSyncpt Method");
            const auto syncpoint_id = static_cast<u32>(data & 0xFF);
            const auto cond = static_cast<u32>((data >> 8) & 0xFF);
            if (cond == 0) {
                sync_manager->Increment(syncpoint_id);
            } else {
                sync_manager->SignalDone(
                    sync_manager->IncrementWhenDone(static_cast<u32>(current_class), syncpoint_id));
            }
            break;
        }
        case ThiMethod::SetMethod1:
            LOG_DEBUG(Service_NVDRV, "VIC method 0x{:X}, Args=({})",
                      static_cast<u32>(vic_thi_state.method_0), data);
            vic_processor->ProcessMethod(static_cast<Vic::Method>(vic_thi_state.method_0), {data});
            break;
        default:
            break;
        }
        break;
    case ChClassId::Host1x:
        // This device is mainly for syncpoint synchronization
        LOG_DEBUG(Service_NVDRV, "Host1X Class Method");
        host1x_processor->ProcessMethod(static_cast<Host1x::Method>(state_offset), {data});
        break;
    default:
        UNIMPLEMENTED_MSG("Current class not implemented {:X}", static_cast<u32>(current_class));
        break;
    }
}

void CDmaPusher::ThiStateWrite(ThiRegisters& state, u32 state_offset,
                               const std::vector<u32>& arguments) {
    u8* const state_offset_ptr = reinterpret_cast<u8*>(&state) + sizeof(u32) * state_offset;
    std::memcpy(state_offset_ptr, arguments.data(), sizeof(u32) * arguments.size());
}

} // namespace Tegra
