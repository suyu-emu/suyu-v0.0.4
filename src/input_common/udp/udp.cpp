// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <mutex>
#include <utility>
#include "common/assert.h"
#include "common/threadsafe_queue.h"
#include "input_common/udp/client.h"
#include "input_common/udp/udp.h"

namespace InputCommon {

class UDPMotion final : public Input::MotionDevice {
public:
    explicit UDPMotion(std::string ip_, int port_, u32 pad_, CemuhookUDP::Client* client_)
        : ip(std::move(ip_)), port(port_), pad(pad_), client(client_) {}

    Input::MotionStatus GetStatus() const override {
        return client->GetPadState(pad).motion_status;
    }

private:
    const std::string ip;
    const int port;
    const u32 pad;
    CemuhookUDP::Client* client;
    mutable std::mutex mutex;
};

/// A motion device factory that creates motion devices from JC Adapter
UDPMotionFactory::UDPMotionFactory(std::shared_ptr<CemuhookUDP::Client> client_)
    : client(std::move(client_)) {}

/**
 * Creates motion device
 * @param params contains parameters for creating the device:
 *     - "port": the nth jcpad on the adapter
 */
std::unique_ptr<Input::MotionDevice> UDPMotionFactory::Create(const Common::ParamPackage& params) {
    auto ip = params.Get("ip", "127.0.0.1");
    const auto port = params.Get("port", 26760);
    const auto pad = static_cast<u32>(params.Get("pad_index", 0));

    return std::make_unique<UDPMotion>(std::move(ip), port, pad, client.get());
}

void UDPMotionFactory::BeginConfiguration() {
    polling = true;
    client->BeginConfiguration();
}

void UDPMotionFactory::EndConfiguration() {
    polling = false;
    client->EndConfiguration();
}

Common::ParamPackage UDPMotionFactory::GetNextInput() {
    Common::ParamPackage params;
    CemuhookUDP::UDPPadStatus pad;
    auto& queue = client->GetPadQueue();
    for (std::size_t pad_number = 0; pad_number < queue.size(); ++pad_number) {
        while (queue[pad_number].Pop(pad)) {
            if (pad.motion == CemuhookUDP::PadMotion::Undefined || std::abs(pad.motion_value) < 1) {
                continue;
            }
            params.Set("engine", "cemuhookudp");
            params.Set("ip", "127.0.0.1");
            params.Set("port", 26760);
            params.Set("pad_index", static_cast<int>(pad_number));
            params.Set("motion", static_cast<u16>(pad.motion));
            return params;
        }
    }
    return params;
}

class UDPTouch final : public Input::TouchDevice {
public:
    explicit UDPTouch(std::string ip_, int port_, u32 pad_, CemuhookUDP::Client* client_)
        : ip(std::move(ip_)), port(port_), pad(pad_), client(client_) {}

    std::tuple<float, float, bool> GetStatus() const override {
        return client->GetPadState(pad).touch_status;
    }

private:
    const std::string ip;
    const int port;
    const u32 pad;
    CemuhookUDP::Client* client;
    mutable std::mutex mutex;
};

/// A motion device factory that creates motion devices from JC Adapter
UDPTouchFactory::UDPTouchFactory(std::shared_ptr<CemuhookUDP::Client> client_)
    : client(std::move(client_)) {}

/**
 * Creates motion device
 * @param params contains parameters for creating the device:
 *     - "port": the nth jcpad on the adapter
 */
std::unique_ptr<Input::TouchDevice> UDPTouchFactory::Create(const Common::ParamPackage& params) {
    auto ip = params.Get("ip", "127.0.0.1");
    const auto port = params.Get("port", 26760);
    const auto pad = static_cast<u32>(params.Get("pad_index", 0));

    return std::make_unique<UDPTouch>(std::move(ip), port, pad, client.get());
}

void UDPTouchFactory::BeginConfiguration() {
    polling = true;
    client->BeginConfiguration();
}

void UDPTouchFactory::EndConfiguration() {
    polling = false;
    client->EndConfiguration();
}

Common::ParamPackage UDPTouchFactory::GetNextInput() {
    Common::ParamPackage params;
    CemuhookUDP::UDPPadStatus pad;
    auto& queue = client->GetPadQueue();
    for (std::size_t pad_number = 0; pad_number < queue.size(); ++pad_number) {
        while (queue[pad_number].Pop(pad)) {
            if (pad.touch == CemuhookUDP::PadTouch::Undefined) {
                continue;
            }
            params.Set("engine", "cemuhookudp");
            params.Set("ip", "127.0.0.1");
            params.Set("port", 26760);
            params.Set("pad_index", static_cast<int>(pad_number));
            params.Set("touch", static_cast<u16>(pad.touch));
            return params;
        }
    }
    return params;
}

} // namespace InputCommon
