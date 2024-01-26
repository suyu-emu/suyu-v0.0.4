// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/service/hle_ipc.h"

namespace Service {

// clang-format off
template <typename T>
class Out {
public:
    using Type = T;

    /* implicit */ Out(Type& t) : raw(&t) {}
    ~Out() = default;

    Type* Get() const {
        return raw;
    }

    Type& operator*() {
        return *raw;
    }

private:
    Type* raw;
};

template <typename T>
using SharedPointer = std::shared_ptr<T>;

struct ClientProcessId {
    explicit operator bool() const {
        return pid != 0;
    }

    const u64& operator*() const {
        return pid;
    }

    u64 pid;
};

struct ProcessId {
    explicit operator bool() const {
        return pid != 0;
    }

    const u64& operator*() const {
        return pid;
    }

    u64 pid;
};

using ClientAppletResourceUserId = ClientProcessId;
using AppletResourceUserId = ProcessId;

template <typename T>
class InCopyHandle {
public:
    using Type = T;

    /* implicit */ InCopyHandle(Type* t) : raw(t) {}
    /* implicit */ InCopyHandle() : raw() {}
    ~InCopyHandle() = default;

    InCopyHandle& operator=(Type* rhs) {
        raw = rhs;
        return *this;
    }

    Type* Get() const {
        return raw;
    }

    Type& operator*() const {
        return *raw;
    }

    Type* operator->() const {
        return raw;
    }

    explicit operator bool() const {
        return raw != nullptr;
    }

private:
    Type* raw;
};

template <typename T>
class OutCopyHandle {
public:
    using Type = T*;

    /* implicit */ OutCopyHandle(Type& t) : raw(&t) {}
    ~OutCopyHandle() = default;

    Type* Get() const {
        return raw;
    }

    Type& operator*() {
        return *raw;
    }

private:
    Type* raw;
};

template <typename T>
class OutMoveHandle {
public:
    using Type = T*;

    /* implicit */ OutMoveHandle(Type& t) : raw(&t) {}
    ~OutMoveHandle() = default;

    Type* Get() const {
        return raw;
    }

    Type& operator*() {
        return *raw;
    }

private:
    Type* raw;
};

enum BufferAttr : int {
    BufferAttr_In = (1U << 0),
    BufferAttr_Out = (1U << 1),
    BufferAttr_HipcMapAlias = (1U << 2),
    BufferAttr_HipcPointer = (1U << 3),
    BufferAttr_FixedSize = (1U << 4),
    BufferAttr_HipcAutoSelect = (1U << 5),
    BufferAttr_HipcMapTransferAllowsNonSecure = (1U << 6),
    BufferAttr_HipcMapTransferAllowsNonDevice = (1U << 7),
};

template <typename T, int A>
struct Buffer : public std::span<T> {
    static_assert(std::is_trivially_copyable_v<T>, "Buffer type must be trivially copyable");
    static_assert((A & BufferAttr_FixedSize) == 0, "Buffer attr must not contain FixedSize");
    static_assert(((A & BufferAttr_In) == 0) ^ ((A & BufferAttr_Out) == 0), "Buffer attr must be In or Out");
    static constexpr BufferAttr Attr = static_cast<BufferAttr>(A);
    using Type = T;

    /* implicit */ Buffer(const std::span<T>& rhs) : std::span<T>(rhs) {}
    /* implicit */ Buffer() = default;

    Buffer& operator=(const std::span<T>& rhs) {
        std::span<T>::operator=(rhs);
        return *this;
    }

    T& operator*() const {
        return *this->data();
    }

    explicit operator bool() const {
        return this->size() > 0;
    }
};

template <BufferAttr A>
using InBuffer = Buffer<const u8, BufferAttr_In | A>;

template <typename T, BufferAttr A>
using InArray = Buffer<T, BufferAttr_In | A>;

template <BufferAttr A>
using OutBuffer = Buffer<u8, BufferAttr_Out | A>;

template <typename T, BufferAttr A>
using OutArray = Buffer<T, BufferAttr_Out | A>;

template <typename T, int A>
struct LargeData : public T {
    static_assert(std::is_trivially_copyable_v<T>, "LargeData type must be trivially copyable");
    static_assert((A & BufferAttr_FixedSize) != 0, "LargeData attr must contain FixedSize");
    static_assert(((A & BufferAttr_In) == 0) ^ ((A & BufferAttr_Out) == 0), "LargeData attr must be In or Out");
    static constexpr BufferAttr Attr = static_cast<BufferAttr>(A);
    using Type = T;

    /* implicit */ LargeData(const T& rhs) : T(rhs) {}
    /* implicit */ LargeData() = default;
};

template <typename T, BufferAttr A>
using InLargeData = LargeData<T, BufferAttr_FixedSize | BufferAttr_In | A>;

template <typename T, BufferAttr A>
using OutLargeData = LargeData<T, BufferAttr_FixedSize | BufferAttr_Out | A>;

template <typename T>
struct RemoveOut {
    using Type = std::remove_reference_t<T>;
};

template <typename T>
struct RemoveOut<Out<T>> {
    using Type = typename Out<T>::Type;
};

template <typename T>
struct RemoveOut<OutCopyHandle<T>> {
    using Type = typename OutCopyHandle<T>::Type;
};

template <typename T>
struct RemoveOut<OutMoveHandle<T>> {
    using Type = typename OutMoveHandle<T>::Type;
};

enum class ArgumentType {
    InProcessId,
    InData,
    InInterface,
    InCopyHandle,
    OutData,
    OutInterface,
    OutCopyHandle,
    OutMoveHandle,
    InBuffer,
    InLargeData,
    OutBuffer,
    OutLargeData,
};

template <typename T>
struct ArgumentTraits;

template <>
struct ArgumentTraits<ClientProcessId> {
    static constexpr ArgumentType Type = ArgumentType::InProcessId;
};

template <typename T>
struct ArgumentTraits<SharedPointer<T>> {
    static constexpr ArgumentType Type = ArgumentType::InInterface;
};

template <typename T>
struct ArgumentTraits<InCopyHandle<T>> {
    static constexpr ArgumentType Type = ArgumentType::InCopyHandle;
};

template <typename T>
struct ArgumentTraits<Out<SharedPointer<T>>> {
    static constexpr ArgumentType Type = ArgumentType::OutInterface;
};

template <typename T>
struct ArgumentTraits<Out<T>> {
    static constexpr ArgumentType Type = ArgumentType::OutData;
};

template <typename T>
struct ArgumentTraits<OutCopyHandle<T>> {
    static constexpr ArgumentType Type = ArgumentType::OutCopyHandle;
};

template <typename T>
struct ArgumentTraits<OutMoveHandle<T>> {
    static constexpr ArgumentType Type = ArgumentType::OutMoveHandle;
};

template <typename T, int A>
struct ArgumentTraits<Buffer<T, A>> {
    static constexpr ArgumentType Type = (A & BufferAttr_In) == 0 ? ArgumentType::OutBuffer : ArgumentType::InBuffer;
};

template <typename T, int A>
struct ArgumentTraits<LargeData<T, A>> {
    static constexpr ArgumentType Type = (A & BufferAttr_In) == 0 ? ArgumentType::OutLargeData : ArgumentType::InLargeData;
};

template <typename T>
struct ArgumentTraits {
    static constexpr ArgumentType Type = ArgumentType::InData;
};
// clang-format on

} // namespace Service
