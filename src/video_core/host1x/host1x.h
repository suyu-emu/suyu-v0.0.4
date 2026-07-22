// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ankerl/unordered_dense.h>
#include <unordered_map>
#include <variant>

#include "common/common_types.h"

// fd types?
#include "video_core/host1x/nvdec.h"
#include "video_core/host1x/vic.h"

#include "common/address_space.h"
#include "video_core/cdma_pusher.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/host1x/syncpoint_manager.h"
#include "video_core/memory_manager.h"

namespace Core {
class System;
} // namespace Core

namespace FFmpeg {
class Frame;
} // namespace FFmpeg

namespace Tegra::Host1x {
class Nvdec;

class FrameQueue {
public:
    struct FrameDevice {
        std::deque<std::pair<u64, std::shared_ptr<FFmpeg::Frame>>> m_presentation_order;
        std::unordered_map<u64, std::shared_ptr<FFmpeg::Frame>> m_decode_order;
    };

    void Open(s32 fd) {
        std::scoped_lock l{m_mutex};
        m_frame_devices.insert_or_assign(fd, FrameDevice{});
    }

    void Close(s32 fd) {
        std::scoped_lock l{m_mutex};
        m_frame_devices.erase(fd);
    }

    s32 VicFindNvdecFdFromOffset(u64 search_offset) {
        std::scoped_lock l{m_mutex};
        for (auto const& [fd, dev] : m_frame_devices) {
            for (auto const& [offset, frame] : dev.m_presentation_order)
                if (offset == search_offset)
                    return fd;
            for (auto const& [offset, frame] : dev.m_decode_order)
                if (offset == search_offset)
                    return fd;
        }
        return -1;
    }

    void PushPresentOrder(s32 fd, u64 offset, std::shared_ptr<FFmpeg::Frame>&& frame) {
        std::scoped_lock l{m_mutex};
        if (auto const it = m_frame_devices.find(fd); it != m_frame_devices.end()) {
            if (it->second.m_presentation_order.size() >= MAX_PRESENT_QUEUE)
                it->second.m_presentation_order.pop_front();
            it->second.m_presentation_order.emplace_back(offset, std::move(frame));
        }
    }

    void PushDecodeOrder(s32 fd, u64 offset, std::shared_ptr<FFmpeg::Frame>&& frame) {
        std::scoped_lock l{m_mutex};
        if (auto const it = m_frame_devices.find(fd); it != m_frame_devices.end()) {
            it->second.m_decode_order.insert_or_assign(offset, std::move(frame));
            if (it->second.m_decode_order.size() > MAX_DECODE_MAP) {
                auto it2 = it->second.m_decode_order.begin();
                std::advance(it2, it->second.m_decode_order.size() - MAX_DECODE_MAP);
                it->second.m_decode_order.erase(it->second.m_decode_order.begin(), it2);
            }
        }
    }

    std::shared_ptr<FFmpeg::Frame> GetFrame(s32 fd, u64 offset) {
        if (fd != -1) {
            std::scoped_lock l{m_mutex};
            if (auto const it = m_frame_devices.find(fd); it != m_frame_devices.end()) {
                if (it->second.m_presentation_order.size() > 0)
                    return GetPresentOrderLocked(fd);
                if (it->second.m_decode_order.size() > 0)
                    return GetDecodeOrderLocked(fd, offset);
            }
        }
        return {};
    }

private:
    std::shared_ptr<FFmpeg::Frame> GetPresentOrderLocked(s32 fd) {
        if (auto const it = m_frame_devices.find(fd); it != m_frame_devices.end()) {
            auto frame = std::move(it->second.m_presentation_order.front().second);
            it->second.m_presentation_order.pop_front();
            return frame;
        }
        return {};
    }

    std::shared_ptr<FFmpeg::Frame> GetDecodeOrderLocked(s32 fd, u64 offset) {
        if (auto const it = m_frame_devices.find(fd); it != m_frame_devices.end()) {
            if (auto const it2 = it->second.m_decode_order.find(offset); it2 != it->second.m_decode_order.end()) {
                // TODO: this "mapped" prevents us from fully embracing ankerl
                return std::move(it->second.m_decode_order.extract(it2).mapped());
            }
        }
        return {};
    }

    std::mutex m_mutex{};
    ankerl::unordered_dense::map<s32, FrameDevice> m_frame_devices;

    static constexpr size_t MAX_PRESENT_QUEUE = 100;
    static constexpr size_t MAX_DECODE_MAP = 200;
};

enum class ChannelType : u32 {
    MsEnc = 0,
    VIC = 1,
    GPU = 2,
    NvDec = 3,
    Display = 4,
    NvJpg = 5,
    TSec = 6,
    Max = 7,
};

class Host1x {
public:
    explicit Host1x(Core::System& system);
    ~Host1x();

    Core::System& System() {
        return system;
    }

    SyncpointManager& GetSyncpointManager() {
        return syncpoint_manager;
    }

    const SyncpointManager& GetSyncpointManager() const {
        return syncpoint_manager;
    }

    Tegra::MaxwellDeviceMemoryManager& MemoryManager() {
        return memory_manager;
    }

    const Tegra::MaxwellDeviceMemoryManager& MemoryManager() const {
        return memory_manager;
    }

    Common::FlatAllocator<u32, 0, 32>& Allocator() {
        return allocator;
    }

    const Common::FlatAllocator<u32, 0, 32>& Allocator() const {
        return allocator;
    }

    void StartDevice(s32 fd, ChannelType type, u32 syncpt);
    void StopDevice(s32 fd, ChannelType type);

    void PushEntries(s32 fd, ChCommandHeaderList&& entries) {
        if (auto const nvdec = std::get_if<Tegra::Host1x::Nvdec>(&devices[fd])) {
            nvdec->PushEntries(std::move(entries));
        } else if (auto const vic = std::get_if<Tegra::Host1x::Vic>(&devices[fd])) {
            vic->PushEntries(std::move(entries));
        }
    }

    Core::System& system;
    SyncpointManager syncpoint_manager;
    Tegra::MaxwellDeviceMemoryManager memory_manager;
    Tegra::MemoryManager gmmu_manager;
    Common::FlatAllocator<u32, 0, 32> allocator;
    FrameQueue frame_queue;
    std::array<std::variant<
        std::monostate,
        Tegra::Host1x::Nvdec,
        Tegra::Host1x::Vic
    >, 1024> devices;
#ifdef YUZU_LEGACY
    std::once_flag nvdec_first_init;
    std::once_flag vic_first_init;
#endif
};

} // namespace Tegra::Host1x
