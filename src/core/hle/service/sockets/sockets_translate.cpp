// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>

#include "common/assert.h"
#include "common/common_types.h"
#include "core/hle/service/sockets/sockets.h"
#include "core/hle/service/sockets/sockets_translate.h"
#include "core/network/network.h"

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
    case Network::Errno::NOTCONN:
        return Errno::NOTCONN;
    default:
        UNIMPLEMENTED_MSG("Unimplemented errno={}", static_cast<int>(value));
        return Errno::SUCCESS;
    }
}

std::pair<s32, Errno> Translate(std::pair<s32, Network::Errno> value) {
    return {value.first, Translate(value.second)};
}

Network::Domain Translate(Domain domain) {
    switch (domain) {
    case Domain::INET:
        return Network::Domain::INET;
    default:
        UNIMPLEMENTED_MSG("Unimplemented domain={}", static_cast<int>(domain));
        return {};
    }
}

Domain Translate(Network::Domain domain) {
    switch (domain) {
    case Network::Domain::INET:
        return Domain::INET;
    default:
        UNIMPLEMENTED_MSG("Unimplemented domain={}", static_cast<int>(domain));
        return {};
    }
}

Network::Type Translate(Type type) {
    switch (type) {
    case Type::STREAM:
        return Network::Type::STREAM;
    case Type::DGRAM:
        return Network::Type::DGRAM;
    default:
        UNIMPLEMENTED_MSG("Unimplemented type={}", static_cast<int>(type));
    }
}

Network::Protocol Translate(Type type, Protocol protocol) {
    switch (protocol) {
    case Protocol::UNSPECIFIED:
        LOG_WARNING(Service, "Unspecified protocol, assuming protocol from type");
        switch (type) {
        case Type::DGRAM:
            return Network::Protocol::UDP;
        case Type::STREAM:
            return Network::Protocol::TCP;
        default:
            return Network::Protocol::TCP;
        }
    case Protocol::TCP:
        return Network::Protocol::TCP;
    case Protocol::UDP:
        return Network::Protocol::UDP;
    default:
        UNIMPLEMENTED_MSG("Unimplemented protocol={}", static_cast<int>(protocol));
        return Network::Protocol::TCP;
    }
}

u16 TranslatePollEventsToHost(u16 flags) {
    u16 result = 0;
    const auto translate = [&result, &flags](u16 from, u16 to) {
        if ((flags & from) != 0) {
            flags &= ~from;
            result |= to;
        }
    };
    translate(POLL_IN, Network::POLL_IN);
    translate(POLL_PRI, Network::POLL_PRI);
    translate(POLL_OUT, Network::POLL_OUT);
    translate(POLL_ERR, Network::POLL_ERR);
    translate(POLL_HUP, Network::POLL_HUP);
    translate(POLL_NVAL, Network::POLL_NVAL);

    UNIMPLEMENTED_IF_MSG(flags != 0, "Unimplemented flags={}", flags);
    return result;
}

u16 TranslatePollEventsToGuest(u16 flags) {
    u16 result = 0;
    const auto translate = [&result, &flags](u16 from, u16 to) {
        if ((flags & from) != 0) {
            flags &= ~from;
            result |= to;
        }
    };

    translate(Network::POLL_IN, POLL_IN);
    translate(Network::POLL_PRI, POLL_PRI);
    translate(Network::POLL_OUT, POLL_OUT);
    translate(Network::POLL_ERR, POLL_ERR);
    translate(Network::POLL_HUP, POLL_HUP);
    translate(Network::POLL_NVAL, POLL_NVAL);

    UNIMPLEMENTED_IF_MSG(flags != 0, "Unimplemented flags={}", flags);
    return result;
}

Network::SockAddrIn Translate(SockAddrIn value) {
    ASSERT(value.len == 0 || value.len == sizeof(value));

    return {
        .family = Translate(static_cast<Domain>(value.family)),
        .ip = value.ip,
        .portno = static_cast<u16>(value.portno >> 8 | value.portno << 8),
    };
}

SockAddrIn Translate(Network::SockAddrIn value) {
    return {
        .len = sizeof(SockAddrIn),
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
        UNIMPLEMENTED_MSG("Unimplemented how={}", static_cast<int>(how));
        return {};
    }
}

} // namespace Service::Sockets
