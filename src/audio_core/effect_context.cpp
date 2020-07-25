// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "audio_core/effect_context.h"

namespace AudioCore {
EffectContext::EffectContext(std::size_t effect_count) : effect_count(effect_count) {
    effects.reserve(effect_count);
    std::generate_n(std::back_inserter(effects), effect_count,
                    [] { return std::make_unique<EffectStubbed>(); });
}
EffectContext::~EffectContext() = default;

std::size_t EffectContext::GetCount() const {
    return effect_count;
}

EffectBase* EffectContext::GetInfo(std::size_t i) {
    return effects.at(i).get();
}

const EffectBase* EffectContext::GetInfo(std::size_t i) const {
    return effects.at(i).get();
}

EffectStubbed::EffectStubbed() : EffectBase::EffectBase() {}
EffectStubbed::~EffectStubbed() = default;

void EffectStubbed::Update(EffectInfo::InParams& in_params) {
    if (in_params.is_new) {
        usage = UsageStatus::New;
    }
}

EffectBase::EffectBase() = default;
EffectBase::~EffectBase() = default;

UsageStatus EffectBase::GetUsage() const {
    return usage;
}

} // namespace AudioCore
