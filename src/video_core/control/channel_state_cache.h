#pragma once

#include <deque>
#include <limits>
#include <unordered_map>

#include "common/common_types.h"

namespace Tegra {

namespace Engines {
class Maxwell3D;
class KeplerCompute;
} // namespace Engines

class MemoryManager;

namespace Control {
struct ChannelState;
}

} // namespace Tegra

namespace VideoCommon {

class ChannelInfo {
public:
    ChannelInfo() = delete;
    ChannelInfo(Tegra::Control::ChannelState& state);
    ChannelInfo(const ChannelInfo& state) = delete;
    ChannelInfo& operator=(const ChannelInfo&) = delete;
    ChannelInfo(ChannelInfo&& other) = default;
    ChannelInfo& operator=(ChannelInfo&& other) = default;

    Tegra::Engines::Maxwell3D& maxwell3d;
    Tegra::Engines::KeplerCompute& kepler_compute;
    Tegra::MemoryManager& gpu_memory;
};

template <class P>
class ChannelSetupCaches {
public:
    /// Operations for seting the channel of execution.

    /// Create channel state.
    void CreateChannel(Tegra::Control::ChannelState& channel);

    /// Bind a channel for execution.
    void BindToChannel(s32 id);

    /// Erase channel's state.
    void EraseChannel(s32 id);

protected:
    static constexpr size_t UNSET_CHANNEL{std::numeric_limits<size_t>::max()};

    std::deque<P> channel_storage;
    std::deque<size_t> free_channel_ids;
    std::unordered_map<s32, size_t> channel_map;

    P* channel_state;
    size_t current_channel_id{UNSET_CHANNEL};
    Tegra::Engines::Maxwell3D* maxwell3d;
    Tegra::Engines::KeplerCompute* kepler_compute;
    Tegra::MemoryManager* gpu_memory;
};

} // namespace VideoCommon
