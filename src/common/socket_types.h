// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "common/common_types.h"

namespace Network {

/// Address families
enum class Domain : u8 {
    Unspecified, ///< Represents 0, used in getaddrinfo hints
    INET,        ///< Address family for IPv4
};

/// Socket types
enum class Type {
    Unspecified, ///< Represents 0, used in getaddrinfo hints
    STREAM,
    DGRAM,
    RAW,
    SEQPACKET,
};

/// Protocol values for sockets
enum class Protocol : u8 {
    Unspecified, ///< Represents 0, usable in various places
    IP,
    ICMP,
    TCP,
    UDP,
    IPV6,
    RAW,
    IGMP,
    GGP,
    IPV4,
    ST,
    EGP,
    PIGP,
    RCCMON,
    NVPII,
    PUP,
    ARGUS,
    EMCON,
    XNET,
    CHAOS,
    MUX,
    MEAS,
    HMP,
    PRM,
    IDP,
    TRUNK1,
    TRUNK2,
    LEAF1,
    LEAF2,
    RDP,
    IRTP,
    TP,
    BLT,
    NSP,
    INP,
    DCCP,
    //TODO: 3PC,
    IDPR,
    XTP,
    DDP,
    CMTP,
    TPXX,
    IL,
    SDRP,
    ROUTING,
    FRAGMENT,
    IDRP,
    RSVP,
    GRE,
    MHRP,
    BHA,
    ESP,
    AH,
    INLSP,
    SWIPE,
    NHRP,
    MOBILE,
    TLSP,
    SKIP,
    ICMPV6,
    NONE,
    DSTOPTS,
    AHIP,
    CFTP,
    HELLO,
    SATEXPAK,
    KRYPTOLAN,
    RVD,
    IPPC,
    ADFS,
    SATMON,
    VISA,
    IPCV,
    CPNX,
    CPHB,
    WSN,
    PVP,
    BRSATMON,
    ND,
    WBMON,
    WBEXPAK,
    EON,
    VMTP,
    SVMTP,
    VINES,
    TTP,
    IGP,
    DGP,
    TCF,
    IGRP,
    OSPFIGP,
    SRPC,
    LARP,
    MTP,
    AX25,
    IPEIP,
    MICP,
    SCCSP,
    ETHERIP,
    ENCAP,
    APES,
    GMTP,
    IPCOMP,
    SCTP,
    MH,
    UDPLITE,
    HIP,
    SHIM6,
    PIM,
    CARP,
    PGM,
    MPLS,
    PFSYNC
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

constexpr u32 FLAG_MSG_PEEK = 0x2;
constexpr u32 FLAG_MSG_DONTWAIT = 0x80;
constexpr u32 FLAG_O_NONBLOCK = 0x800;

/// Cross-platform addrinfo structure
struct AddrInfo {
    Domain family;
    Type socket_type;
    Protocol protocol;
    SockAddrIn addr;
    std::optional<std::string> canon_name;
};

} // namespace Network
