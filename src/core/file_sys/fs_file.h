// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace FileSys {

struct ReadOption {
    u32 _value;

    static const ReadOption None;
};

enum ReadOptionFlag : u32 {
    ReadOptionFlag_None = (0 << 0),
};

inline constexpr const ReadOption ReadOption::None = {ReadOptionFlag_None};

inline constexpr bool operator==(const ReadOption& lhs, const ReadOption& rhs) {
    return lhs._value == rhs._value;
}

inline constexpr bool operator!=(const ReadOption& lhs, const ReadOption& rhs) {
    return !(lhs == rhs);
}

static_assert(sizeof(ReadOption) == sizeof(u32));

enum WriteOptionFlag : u32 {
    WriteOptionFlag_None = (0 << 0),
    WriteOptionFlag_Flush = (1 << 0),
};

struct WriteOption {
    u32 _value;

    constexpr inline bool HasFlushFlag() const {
        return _value & WriteOptionFlag_Flush;
    }

    static const WriteOption None;
    static const WriteOption Flush;
};

inline constexpr const WriteOption WriteOption::None = {WriteOptionFlag_None};
inline constexpr const WriteOption WriteOption::Flush = {WriteOptionFlag_Flush};

inline constexpr bool operator==(const WriteOption& lhs, const WriteOption& rhs) {
    return lhs._value == rhs._value;
}

inline constexpr bool operator!=(const WriteOption& lhs, const WriteOption& rhs) {
    return !(lhs == rhs);
}

static_assert(sizeof(WriteOption) == sizeof(u32));

struct FileHandle {
    void* handle;
};

} // namespace FileSys
