// Copyright 2021 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <boost/intrusive/list.hpp>

#include "common/common_types.h"
#include "core/hle/kernel/k_server_session.h"
#include "core/hle/kernel/k_synchronization_object.h"
#include "core/hle/result.h"

namespace Kernel {

class KernelCore;
class KPort;
class SessionRequestHandler;

class KServerPort final : public KSynchronizationObject {
    KERNEL_AUTOOBJECT_TRAITS(KServerPort, KSynchronizationObject);

private:
    using SessionList = boost::intrusive::list<KServerSession>;

public:
    explicit KServerPort(KernelCore& kernel_);
    virtual ~KServerPort() override;

    using HLEHandler = std::shared_ptr<SessionRequestHandler>;

    void Initialize(KPort* parent_, std::string&& name_);

    /// Whether or not this server port has an HLE handler available.
    bool HasHLEHandler() const {
        return hle_handler != nullptr;
    }

    /// Gets the HLE handler for this port.
    HLEHandler GetHLEHandler() const {
        return hle_handler;
    }

    /**
     * Sets the HLE handler template for the port. ServerSessions crated by connecting to this port
     * will inherit a reference to this handler.
     */
    void SetHleHandler(HLEHandler hle_handler_) {
        hle_handler = std::move(hle_handler_);
    }

    void EnqueueSession(KServerSession* pending_session);

    KServerSession* AcceptSession();

    const KPort* GetParent() const {
        return parent;
    }

    bool IsLight() const;

    // Overridden virtual functions.
    virtual void Destroy() override;
    virtual bool IsSignaled() const override;

private:
    void CleanupSessions();

private:
    SessionList session_list;
    HLEHandler hle_handler;
    KPort* parent{};
};

} // namespace Kernel
