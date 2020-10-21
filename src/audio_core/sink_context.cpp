// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/sink_context.h"

namespace AudioCore {
SinkContext::SinkContext(std::size_t sink_count) : sink_count(sink_count) {}
SinkContext::~SinkContext() = default;

std::size_t SinkContext::GetCount() const {
    return sink_count;
}

void SinkContext::UpdateMainSink(SinkInfo::InParams& in) {
    in_use = in.in_use;
    use_count = in.device.input_count;
    std::memcpy(buffers.data(), in.device.input.data(), AudioCommon::MAX_CHANNEL_COUNT);
}

bool SinkContext::InUse() const {
    return in_use;
}

std::vector<u8> SinkContext::OutputBuffers() const {
    std::vector<u8> buffer_ret(use_count);
    std::memcpy(buffer_ret.data(), buffers.data(), use_count);
    return buffer_ret;
}

} // namespace AudioCore
