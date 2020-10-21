// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <utility>

#include "common/common_types.h"

namespace Network {

class Socket;

/// Error code for network functions
enum class Errno {
    SUCCESS,
    BADF,
    INVAL,
    MFILE,
    NOTCONN,
    AGAIN,
};

/// Address families
enum class Domain {
    INET, ///< Address family for IPv4
};

/// Socket types
enum class Type {
    STREAM,
    DGRAM,
    RAW,
    SEQPACKET,
};

/// Protocol values for sockets
enum class Protocol {
    ICMP,
    TCP,
    UDP,
};

/// Shutdown mode
enum class ShutdownHow {
    RD,
    WR,
    RDWR,
};

/// Array of IPv4 address
using IPv4Address = std::array<u8, 4>;

/// Cross-platform sockaddr structure
struct SockAddrIn {
    Domain family;
    IPv4Address ip;
    u16 portno;
};

/// Cross-platform poll fd structure
struct PollFD {
    Socket* socket;
    u16 events;
    u16 revents;
};

constexpr u16 POLL_IN = 1 << 0;
constexpr u16 POLL_PRI = 1 << 1;
constexpr u16 POLL_OUT = 1 << 2;
constexpr u16 POLL_ERR = 1 << 3;
constexpr u16 POLL_HUP = 1 << 4;
constexpr u16 POLL_NVAL = 1 << 5;

class NetworkInstance {
public:
    explicit NetworkInstance();
    ~NetworkInstance();
};

/// @brief Returns host's IPv4 address
/// @return Pair of an array of human ordered IPv4 address (e.g. 192.168.0.1) and an error code
std::pair<IPv4Address, Errno> GetHostIPv4Address();

} // namespace Network
