// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Sockets {

enum class Errno : u32 {
    SUCCESS = 0,
    BADF = 9,
    AGAIN = 11,
    INVAL = 22,
    MFILE = 24,
    NOTCONN = 107,
};

enum class Domain : u32 {
    INET = 2,
};

enum class Type : u32 {
    STREAM = 1,
    DGRAM = 2,
    RAW = 3,
    SEQPACKET = 5,
};

enum class Protocol : u32 {
    UNSPECIFIED = 0,
    ICMP = 1,
    TCP = 6,
    UDP = 17,
};

enum class OptName : u32 {
    REUSEADDR = 0x4,
    BROADCAST = 0x20,
    LINGER = 0x80,
    SNDBUF = 0x1001,
    RCVBUF = 0x1002,
    SNDTIMEO = 0x1005,
    RCVTIMEO = 0x1006,
};

enum class ShutdownHow : s32 {
    RD = 0,
    WR = 1,
    RDWR = 2,
};

enum class FcntlCmd : s32 {
    GETFL = 3,
    SETFL = 4,
};

struct SockAddrIn {
    u8 len;
    u8 family;
    u16 portno;
    std::array<u8, 4> ip;
    std::array<u8, 8> zeroes;
};

struct PollFD {
    s32 fd;
    u16 events;
    u16 revents;
};

struct Linger {
    u32 onoff;
    u32 linger;
};

constexpr u16 POLL_IN = 0x01;
constexpr u16 POLL_PRI = 0x02;
constexpr u16 POLL_OUT = 0x04;
constexpr u16 POLL_ERR = 0x08;
constexpr u16 POLL_HUP = 0x10;
constexpr u16 POLL_NVAL = 0x20;

constexpr u32 FLAG_MSG_DONTWAIT = 0x80;

constexpr u32 FLAG_O_NONBLOCK = 0x800;

/// Registers all Sockets services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager, Core::System& system);

} // namespace Service::Sockets
