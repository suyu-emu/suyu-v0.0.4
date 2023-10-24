// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <vector>

#include "common/concepts.h"
#include "core/hle/service/nvdrv/devices/nvdevice.h"

namespace Service::Nvidia::Devices {

struct Ioctl1Traits {
    template <typename T, typename R, typename A>
    static T GetClassImpl(R (T::*)(A));

    template <typename T, typename R, typename A>
    static A GetArgImpl(R (T::*)(A));
};

struct Ioctl23Traits {
    template <typename T, typename R, typename A, typename B>
    static T GetClassImpl(R (T::*)(A, B));

    template <typename T, typename R, typename A, typename B>
    static A GetArgImpl(R (T::*)(A, B));
};

template <typename T>
struct ContainerType {
    using ValueType = T;
};

template <Common::IsContiguousContainer T>
struct ContainerType<T> {
    using ValueType = T::value_type;
};

template <typename InnerArg, typename F, typename Self, typename... Rest>
NvResult Wrap(std::span<const u8> input, std::span<u8> output, Self* self, F&& callable,
              Rest&&... rest) {
    using Arg = ContainerType<InnerArg>::ValueType;
    constexpr bool ArgumentIsContainer = Common::IsContiguousContainer<InnerArg>;

    // Verify that the input and output sizes are valid.
    const size_t in_params = input.size() / sizeof(Arg);
    const size_t out_params = output.size() / sizeof(Arg);
    if (in_params * sizeof(Arg) != input.size()) {
        return NvResult::InvalidSize;
    }
    if (out_params * sizeof(Arg) != output.size()) {
        return NvResult::InvalidSize;
    }
    if (in_params == 0 && out_params == 0 && !ArgumentIsContainer) {
        return NvResult::InvalidSize;
    }

    // Copy inputs, if needed.
    std::vector<Arg> params(std::max(in_params, out_params));
    if (in_params > 0) {
        std::memcpy(params.data(), input.data(), input.size());
    }

    // Perform the call.
    NvResult result;
    if constexpr (ArgumentIsContainer) {
        result = (self->*callable)(params, std::forward<Rest>(rest)...);
    } else {
        result = (self->*callable)(params.front(), std::forward<Rest>(rest)...);
    }

    // Copy outputs, if needed.
    if (out_params > 0) {
        std::memcpy(output.data(), params.data(), output.size());
    }

    return result;
}

template <typename F>
NvResult nvdevice::Wrap1(F&& callable, std::span<const u8> input, std::span<u8> output) {
    using Self = decltype(Ioctl1Traits::GetClassImpl(callable));
    using InnerArg = std::remove_reference_t<decltype(Ioctl1Traits::GetArgImpl(callable))>;

    return Wrap<InnerArg>(input, output, static_cast<Self*>(this), callable);
}

template <typename F>
NvResult nvdevice::Wrap2(F&& callable, std::span<const u8> input, std::span<const u8> inline_input,
                         std::span<u8> output) {
    using Self = decltype(Ioctl23Traits::GetClassImpl(callable));
    using InnerArg = std::remove_reference_t<decltype(Ioctl23Traits::GetArgImpl(callable))>;

    return Wrap<InnerArg>(input, output, static_cast<Self*>(this), callable, inline_input);
}

template <typename F>
NvResult nvdevice::Wrap3(F&& callable, std::span<const u8> input, std::span<u8> output,
                         std::span<u8> inline_output) {
    using Self = decltype(Ioctl23Traits::GetClassImpl(callable));
    using InnerArg = std::remove_reference_t<decltype(Ioctl23Traits::GetArgImpl(callable))>;

    return Wrap<InnerArg>(input, output, static_cast<Self*>(this), callable, inline_output);
}

} // namespace Service::Nvidia::Devices
