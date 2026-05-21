// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <utility>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging.h"
#include "core/hle/service/sockets/sockets.h"
#include "core/hle/service/sockets/sockets_translate.h"
#include "core/internal_network/network.h"

namespace Service::Sockets {

Errno Translate(Network::Errno value) {
    switch (value) {
    case Network::Errno::SUCCESS:
        return Errno::SUCCESS;
    case Network::Errno::BADF:
        return Errno::BADF;
    case Network::Errno::AGAIN:
        return Errno::AGAIN;
    case Network::Errno::INVAL:
        return Errno::INVAL;
    case Network::Errno::MFILE:
        return Errno::MFILE;
    case Network::Errno::PIPE:
        return Errno::PIPE;
    case Network::Errno::CONNREFUSED:
        return Errno::CONNREFUSED;
    case Network::Errno::NOTCONN:
        return Errno::NOTCONN;
    case Network::Errno::TIMEDOUT:
        return Errno::TIMEDOUT;
    case Network::Errno::CONNABORTED:
        return Errno::CONNABORTED;
    case Network::Errno::CONNRESET:
        return Errno::CONNRESET;
    case Network::Errno::INPROGRESS:
        return Errno::INPROGRESS;
    case Network::Errno::ISCONN:
        return Errno::ISCONN;
    default:
        UNIMPLEMENTED_MSG("Unimplemented errno={}", value);
        return Errno::SUCCESS;
    }
}

std::pair<s32, Errno> Translate(std::pair<s32, Network::Errno> value) {
    return {value.first, Translate(value.second)};
}

GetAddrInfoError Translate(Network::GetAddrInfoError error) {
    switch (error) {
    case Network::GetAddrInfoError::SUCCESS:
        return GetAddrInfoError::SUCCESS;
    case Network::GetAddrInfoError::ADDRFAMILY:
        return GetAddrInfoError::ADDRFAMILY;
    case Network::GetAddrInfoError::AGAIN:
        return GetAddrInfoError::AGAIN;
    case Network::GetAddrInfoError::BADFLAGS:
        return GetAddrInfoError::BADFLAGS;
    case Network::GetAddrInfoError::FAIL:
        return GetAddrInfoError::FAIL;
    case Network::GetAddrInfoError::FAMILY:
        return GetAddrInfoError::FAMILY;
    case Network::GetAddrInfoError::MEMORY:
        return GetAddrInfoError::MEMORY;
    case Network::GetAddrInfoError::NODATA:
        return GetAddrInfoError::NODATA;
    case Network::GetAddrInfoError::NONAME:
        return GetAddrInfoError::NONAME;
    case Network::GetAddrInfoError::SERVICE:
        return GetAddrInfoError::SERVICE;
    case Network::GetAddrInfoError::SOCKTYPE:
        return GetAddrInfoError::SOCKTYPE;
    case Network::GetAddrInfoError::SYSTEM:
        return GetAddrInfoError::SYSTEM;
    case Network::GetAddrInfoError::BADHINTS:
        return GetAddrInfoError::BADHINTS;
    case Network::GetAddrInfoError::PROTOCOL:
        return GetAddrInfoError::PROTOCOL;
    case Network::GetAddrInfoError::OVERFLOW_:
        return GetAddrInfoError::OVERFLOW_;
    case Network::GetAddrInfoError::OTHER:
        return GetAddrInfoError::OTHER;
    default:
        UNIMPLEMENTED_MSG("Unimplemented GetAddrInfoError={}", error);
        return GetAddrInfoError::OTHER;
    }
}

const char* Translate(GetAddrInfoError error) {
    // https://android.googlesource.com/platform/bionic/+/085543106/libc/dns/net/getaddrinfo.c#254
    switch (error) {
    case GetAddrInfoError::SUCCESS:
        return "Success";
    case GetAddrInfoError::ADDRFAMILY:
        return "Address family for hostname not supported";
    case GetAddrInfoError::AGAIN:
        return "Temporary failure in name resolution";
    case GetAddrInfoError::BADFLAGS:
        return "Invalid value for ai_flags";
    case GetAddrInfoError::FAIL:
        return "Non-recoverable failure in name resolution";
    case GetAddrInfoError::FAMILY:
        return "ai_family not supported";
    case GetAddrInfoError::MEMORY:
        return "Memory allocation failure";
    case GetAddrInfoError::NODATA:
        return "No address associated with hostname";
    case GetAddrInfoError::NONAME:
        return "hostname nor servname provided, or not known";
    case GetAddrInfoError::SERVICE:
        return "servname not supported for ai_socktype";
    case GetAddrInfoError::SOCKTYPE:
        return "ai_socktype not supported";
    case GetAddrInfoError::SYSTEM:
        return "System error returned in errno";
    case GetAddrInfoError::BADHINTS:
        return "Invalid value for hints";
    case GetAddrInfoError::PROTOCOL:
        return "Resolved protocol is unknown";
    case GetAddrInfoError::OVERFLOW_:
        return "Argument buffer overflow";
    default:
        return "Unknown error";
    }
}

Network::Domain Translate(Domain domain) {
    switch (domain) {
    case Domain::Unspecified:
        return Network::Domain::Unspecified;
    case Domain::INET:
        return Network::Domain::INET;
    default:
        UNIMPLEMENTED_MSG("Unimplemented domain={}", domain);
        return {};
    }
}

Domain Translate(Network::Domain domain) {
    switch (domain) {
    case Network::Domain::Unspecified:
        return Domain::Unspecified;
    case Network::Domain::INET:
        return Domain::INET;
    default:
        UNIMPLEMENTED_MSG("Unimplemented domain={}", domain);
        return {};
    }
}

Network::Type Translate(Type type) {
    switch (type) {
    case Type::Unspecified:
        return Network::Type::Unspecified;
    case Type::STREAM:
        return Network::Type::STREAM;
    case Type::DGRAM:
        return Network::Type::DGRAM;
    case Type::RAW:
        return Network::Type::RAW;
    case Type::SEQPACKET:
        return Network::Type::SEQPACKET;
    default:
        UNIMPLEMENTED_MSG("Unimplemented type={}", type);
        return Network::Type{};
    }
}

Type Translate(Network::Type type) {
    switch (type) {
    case Network::Type::Unspecified: return Type::Unspecified;
    case Network::Type::STREAM: return Type::STREAM;
    case Network::Type::DGRAM: return Type::DGRAM;
    case Network::Type::RAW: return Type::RAW;
    case Network::Type::SEQPACKET: return Type::SEQPACKET;
    default:
        UNIMPLEMENTED_MSG("Unimplemented type={}", type);
        return Type{};
    }
}

#define NETWORK_PROTOCOL_TRANSLATE_LIST \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ICMP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(TCP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(UDP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IPV6) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(RAW) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IGMP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(GGP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IPV4) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ST) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(EGP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(PIGP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(RCCMON) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(NVPII) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(PUP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ARGUS) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(EMCON) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(XNET) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(CHAOS) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(MUX) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(MEAS) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(HMP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(PRM) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IDP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(TRUNK1) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(TRUNK2) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(LEAF1) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(LEAF2) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(RDP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IRTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(TP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(BLT) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(NSP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(INP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(DCCP) \
    /*NETWORK_PROTOCOL_TRANSLATE_ELEM(3PC)*/ \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IDPR) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(XTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(DDP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(CMTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(TPXX) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IL) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SDRP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ROUTING) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(FRAGMENT) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IDRP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(RSVP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(GRE) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(MHRP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(BHA) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ESP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(AH) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(INLSP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SWIPE) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(NHRP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(MOBILE) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(TLSP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SKIP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ICMPV6) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(NONE) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(DSTOPTS) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(AHIP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(CFTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(HELLO) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SATEXPAK) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(KRYPTOLAN) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(RVD) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IPPC) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ADFS) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SATMON) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(VISA) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IPCV) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(CPNX) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(CPHB) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(WSN) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(PVP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(BRSATMON) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ND) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(WBMON) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(WBEXPAK) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(EON) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(VMTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SVMTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(VINES) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(TTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IGP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(DGP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(TCF) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IGRP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(OSPFIGP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SRPC) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(LARP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(MTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(AX25) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IPEIP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(MICP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SCCSP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ETHERIP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(ENCAP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(APES) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(GMTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(IPCOMP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SCTP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(MH) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(UDPLITE) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(HIP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(SHIM6) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(PIM) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(CARP) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(PGM) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(MPLS) \
    NETWORK_PROTOCOL_TRANSLATE_ELEM(PFSYNC)
[[nodiscard]] Network::Protocol Translate(Protocol protocol) {
    switch (protocol) {
#define NETWORK_PROTOCOL_TRANSLATE_ELEM(name) case Protocol::name: return Network::Protocol::name;
    NETWORK_PROTOCOL_TRANSLATE_LIST
#undef NETWORK_PROTOCOL_TRANSLATE_ELEM
    default:
        UNIMPLEMENTED_MSG("Unimplemented protocol={}", protocol);
        return {};
    }
}
[[nodiscard]] Protocol Translate(Network::Protocol protocol) {
    switch (protocol) {
#define NETWORK_PROTOCOL_TRANSLATE_ELEM(name) case Network::Protocol::name: return Protocol::name;
    NETWORK_PROTOCOL_TRANSLATE_LIST
#undef NETWORK_PROTOCOL_TRANSLATE_ELEM
    default:
        UNIMPLEMENTED_MSG("Unimplemented protocol={}", protocol);
        return {};
    }
}
#undef NETWORK_PROTOCOL_TRANSLATE_LIST

Network::PollEvents Translate(PollEvents flags) {
    Network::PollEvents result{};
    const auto translate = [&result, &flags](PollEvents from, Network::PollEvents to) {
        if (True(flags & from)) {
            flags &= ~from;
            result |= to;
        }
    };
    translate(PollEvents::In, Network::PollEvents::In);
    translate(PollEvents::Pri, Network::PollEvents::Pri);
    translate(PollEvents::Out, Network::PollEvents::Out);
    translate(PollEvents::Err, Network::PollEvents::Err);
    translate(PollEvents::Hup, Network::PollEvents::Hup);
    translate(PollEvents::Nval, Network::PollEvents::Nval);
    translate(PollEvents::RdNorm, Network::PollEvents::RdNorm);
    translate(PollEvents::RdBand, Network::PollEvents::RdBand);
    translate(PollEvents::WrBand, Network::PollEvents::WrBand);

    UNIMPLEMENTED_IF_MSG((u16)flags != 0, "Unimplemented flags={}", (u16)flags);
    return result;
}

PollEvents Translate(Network::PollEvents flags) {
    PollEvents result{};
    const auto translate = [&result, &flags](Network::PollEvents from, PollEvents to) {
        if (True(flags & from)) {
            flags &= ~from;
            result |= to;
        }
    };

    translate(Network::PollEvents::In, PollEvents::In);
    translate(Network::PollEvents::Pri, PollEvents::Pri);
    translate(Network::PollEvents::Out, PollEvents::Out);
    translate(Network::PollEvents::Err, PollEvents::Err);
    translate(Network::PollEvents::Hup, PollEvents::Hup);
    translate(Network::PollEvents::Nval, PollEvents::Nval);
    translate(Network::PollEvents::RdNorm, PollEvents::RdNorm);
    translate(Network::PollEvents::RdBand, PollEvents::RdBand);
    translate(Network::PollEvents::WrBand, PollEvents::WrBand);

    UNIMPLEMENTED_IF_MSG((u16)flags != 0, "Unimplemented flags={}", (u16)flags);
    return result;
}

Network::SockAddrIn Translate(SockAddrIn value) {
    // All lengths are valid, from [0 upto 256]
    return {
        .family = Translate(Domain(value.family)),
        .ip = value.ip,
        .portno = static_cast<u16>(value.portno >> 8 | value.portno << 8),
    };
}

SockAddrIn Translate(Network::SockAddrIn value) {
    return {
        .len = 16,
        .family = static_cast<u8>(Translate(value.family)),
        .portno = static_cast<u16>(value.portno >> 8 | value.portno << 8),
        .ip = value.ip,
        .zeroes = {},
    };
}

Network::ShutdownHow Translate(ShutdownHow how) {
    switch (how) {
    case ShutdownHow::RD:
        return Network::ShutdownHow::RD;
    case ShutdownHow::WR:
        return Network::ShutdownHow::WR;
    case ShutdownHow::RDWR:
        return Network::ShutdownHow::RDWR;
    default:
        UNIMPLEMENTED_MSG("Unimplemented how={}", how);
        return {};
    }
}

} // namespace Service::Sockets
