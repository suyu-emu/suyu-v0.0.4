// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <cstring>
#include <functional>
#include <thread>
#include <boost/asio.hpp>
#include "common/logging/log.h"
#include "core/settings.h"
#include "input_common/udp/client.h"
#include "input_common/udp/protocol.h"

using boost::asio::ip::udp;

namespace InputCommon::CemuhookUDP {

struct SocketCallback {
    std::function<void(Response::Version)> version;
    std::function<void(Response::PortInfo)> port_info;
    std::function<void(Response::PadData)> pad_data;
};

class Socket {
public:
    using clock = std::chrono::system_clock;

    explicit Socket(const std::string& host, u16 port, std::size_t pad_index_, u32 client_id_,
                    SocketCallback callback_)
        : callback(std::move(callback_)), timer(io_service),
          socket(io_service, udp::endpoint(udp::v4(), 0)), client_id(client_id_),
          pad_index(pad_index_) {
        boost::system::error_code ec{};
        auto ipv4 = boost::asio::ip::make_address_v4(host, ec);
        if (ec.value() != boost::system::errc::success) {
            LOG_ERROR(Input, "Invalid IPv4 address \"{}\" provided to socket", host);
            ipv4 = boost::asio::ip::address_v4{};
        }

        send_endpoint = {udp::endpoint(ipv4, port)};
    }

    void Stop() {
        io_service.stop();
    }

    void Loop() {
        io_service.run();
    }

    void StartSend(const clock::time_point& from) {
        timer.expires_at(from + std::chrono::seconds(3));
        timer.async_wait([this](const boost::system::error_code& error) { HandleSend(error); });
    }

    void StartReceive() {
        socket.async_receive_from(
            boost::asio::buffer(receive_buffer), receive_endpoint,
            [this](const boost::system::error_code& error, std::size_t bytes_transferred) {
                HandleReceive(error, bytes_transferred);
            });
    }

private:
    void HandleReceive(const boost::system::error_code& error, std::size_t bytes_transferred) {
        if (auto type = Response::Validate(receive_buffer.data(), bytes_transferred)) {
            switch (*type) {
            case Type::Version: {
                Response::Version version;
                std::memcpy(&version, &receive_buffer[sizeof(Header)], sizeof(Response::Version));
                callback.version(std::move(version));
                break;
            }
            case Type::PortInfo: {
                Response::PortInfo port_info;
                std::memcpy(&port_info, &receive_buffer[sizeof(Header)],
                            sizeof(Response::PortInfo));
                callback.port_info(std::move(port_info));
                break;
            }
            case Type::PadData: {
                Response::PadData pad_data;
                std::memcpy(&pad_data, &receive_buffer[sizeof(Header)], sizeof(Response::PadData));
                callback.pad_data(std::move(pad_data));
                break;
            }
            }
        }
        StartReceive();
    }

    void HandleSend(const boost::system::error_code& error) {
        boost::system::error_code _ignored{};
        // Send a request for getting port info for the pad
        const Request::PortInfo port_info{1, {static_cast<u8>(pad_index), 0, 0, 0}};
        const auto port_message = Request::Create(port_info, client_id);
        std::memcpy(&send_buffer1, &port_message, PORT_INFO_SIZE);
        socket.send_to(boost::asio::buffer(send_buffer1), send_endpoint, {}, _ignored);

        // Send a request for getting pad data for the pad
        const Request::PadData pad_data{
            Request::PadData::Flags::Id,
            static_cast<u8>(pad_index),
            EMPTY_MAC_ADDRESS,
        };
        const auto pad_message = Request::Create(pad_data, client_id);
        std::memcpy(send_buffer2.data(), &pad_message, PAD_DATA_SIZE);
        socket.send_to(boost::asio::buffer(send_buffer2), send_endpoint, {}, _ignored);
        StartSend(timer.expiry());
    }

    SocketCallback callback;
    boost::asio::io_service io_service;
    boost::asio::basic_waitable_timer<clock> timer;
    udp::socket socket;

    u32 client_id{};
    std::size_t pad_index{};

    static constexpr std::size_t PORT_INFO_SIZE = sizeof(Message<Request::PortInfo>);
    static constexpr std::size_t PAD_DATA_SIZE = sizeof(Message<Request::PadData>);
    std::array<u8, PORT_INFO_SIZE> send_buffer1;
    std::array<u8, PAD_DATA_SIZE> send_buffer2;
    udp::endpoint send_endpoint;

    std::array<u8, MAX_PACKET_SIZE> receive_buffer;
    udp::endpoint receive_endpoint;
};

static void SocketLoop(Socket* socket) {
    socket->StartReceive();
    socket->StartSend(Socket::clock::now());
    socket->Loop();
}

Client::Client() {
    LOG_INFO(Input, "Udp Initialization started");
    for (std::size_t client = 0; client < clients.size(); client++) {
        const auto pad = client % 4;
        StartCommunication(client, Settings::values.udp_input_address,
                           Settings::values.udp_input_port, pad, 24872);
        // Set motion parameters
        // SetGyroThreshold value should be dependent on GyroscopeZeroDriftMode
        // Real HW values are unknown, 0.0001 is an approximate to Standard
        clients[client].motion.SetGyroThreshold(0.0001f);
    }
}

Client::~Client() {
    Reset();
}

std::vector<Common::ParamPackage> Client::GetInputDevices() const {
    std::vector<Common::ParamPackage> devices;
    for (std::size_t client = 0; client < clients.size(); client++) {
        if (!DeviceConnected(client)) {
            continue;
        }
        std::string name = fmt::format("UDP Controller {}", client);
        devices.emplace_back(Common::ParamPackage{
            {"class", "cemuhookudp"},
            {"display", std::move(name)},
            {"port", std::to_string(client)},
        });
    }
    return devices;
}

bool Client::DeviceConnected(std::size_t pad) const {
    // Use last timestamp to detect if the socket has stopped sending data
    const auto now = std::chrono::system_clock::now();
    const auto time_difference = static_cast<u64>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - clients[pad].last_motion_update)
            .count());
    return time_difference < 1000 && clients[pad].active == 1;
}

void Client::ReloadUDPClient() {
    for (std::size_t client = 0; client < clients.size(); client++) {
        ReloadSocket(Settings::values.udp_input_address, Settings::values.udp_input_port, client);
    }
}
void Client::ReloadSocket(const std::string& host, u16 port, std::size_t pad_index, u32 client_id) {
    // client number must be determined from host / port and pad index
    const std::size_t client = pad_index;
    clients[client].socket->Stop();
    clients[client].thread.join();
    StartCommunication(client, host, port, pad_index, client_id);
}

void Client::OnVersion(Response::Version data) {
    LOG_TRACE(Input, "Version packet received: {}", data.version);
}

void Client::OnPortInfo(Response::PortInfo data) {
    LOG_TRACE(Input, "PortInfo packet received: {}", data.model);
}

void Client::OnPadData(Response::PadData data) {
    // Client number must be determined from host / port and pad index
    const std::size_t client = data.info.id;
    LOG_TRACE(Input, "PadData packet received");
    if (data.packet_counter == clients[client].packet_sequence) {
        LOG_WARNING(
            Input,
            "PadData packet dropped because its stale info. Current count: {} Packet count: {}",
            clients[client].packet_sequence, data.packet_counter);
        return;
    }
    clients[client].active = data.info.is_pad_active;
    clients[client].packet_sequence = data.packet_counter;
    const auto now = std::chrono::system_clock::now();
    const auto time_difference =
        static_cast<u64>(std::chrono::duration_cast<std::chrono::microseconds>(
                             now - clients[client].last_motion_update)
                             .count());
    clients[client].last_motion_update = now;
    const Common::Vec3f raw_gyroscope = {data.gyro.pitch, data.gyro.roll, -data.gyro.yaw};
    clients[client].motion.SetAcceleration({data.accel.x, -data.accel.z, data.accel.y});
    // Gyroscope values are not it the correct scale from better joy.
    // Dividing by 312 allows us to make one full turn = 1 turn
    // This must be a configurable valued called sensitivity
    clients[client].motion.SetGyroscope(raw_gyroscope / 312.0f);
    clients[client].motion.UpdateRotation(time_difference);
    clients[client].motion.UpdateOrientation(time_difference);

    {
        std::lock_guard guard(clients[client].status.update_mutex);
        clients[client].status.motion_status = clients[client].motion.GetMotion();

        // TODO: add a setting for "click" touch. Click touch refers to a device that differentiates
        // between a simple "tap" and a hard press that causes the touch screen to click.
        const bool is_active = data.touch_1.is_active != 0;

        float x = 0;
        float y = 0;

        if (is_active && clients[client].status.touch_calibration) {
            const u16 min_x = clients[client].status.touch_calibration->min_x;
            const u16 max_x = clients[client].status.touch_calibration->max_x;
            const u16 min_y = clients[client].status.touch_calibration->min_y;
            const u16 max_y = clients[client].status.touch_calibration->max_y;

            x = static_cast<float>(std::clamp(static_cast<u16>(data.touch_1.x), min_x, max_x) -
                                   min_x) /
                static_cast<float>(max_x - min_x);
            y = static_cast<float>(std::clamp(static_cast<u16>(data.touch_1.y), min_y, max_y) -
                                   min_y) /
                static_cast<float>(max_y - min_y);
        }

        clients[client].status.touch_status = {x, y, is_active};

        if (configuring) {
            const Common::Vec3f gyroscope = clients[client].motion.GetGyroscope();
            const Common::Vec3f accelerometer = clients[client].motion.GetAcceleration();
            UpdateYuzuSettings(client, accelerometer, gyroscope, is_active);
        }
    }
}

void Client::StartCommunication(std::size_t client, const std::string& host, u16 port,
                                std::size_t pad_index, u32 client_id) {
    SocketCallback callback{[this](Response::Version version) { OnVersion(version); },
                            [this](Response::PortInfo info) { OnPortInfo(info); },
                            [this](Response::PadData data) { OnPadData(data); }};
    LOG_INFO(Input, "Starting communication with UDP input server on {}:{}", host, port);
    clients[client].socket = std::make_unique<Socket>(host, port, pad_index, client_id, callback);
    clients[client].thread = std::thread{SocketLoop, clients[client].socket.get()};
}

void Client::Reset() {
    for (auto& client : clients) {
        client.socket->Stop();
        client.thread.join();
    }
}

void Client::UpdateYuzuSettings(std::size_t client, const Common::Vec3<float>& acc,
                                const Common::Vec3<float>& gyro, bool touch) {
    if (gyro.Length() > 0.2f) {
        LOG_DEBUG(Input, "UDP Controller {}: gyro=({}, {}, {}), accel=({}, {}, {}), touch={}",
                  client, gyro[0], gyro[1], gyro[2], acc[0], acc[1], acc[2], touch);
    }
    UDPPadStatus pad;
    if (touch) {
        pad.touch = PadTouch::Click;
        pad_queue[client].Push(pad);
    }
    for (size_t i = 0; i < 3; ++i) {
        if (gyro[i] > 5.0f || gyro[i] < -5.0f) {
            pad.motion = static_cast<PadMotion>(i);
            pad.motion_value = gyro[i];
            pad_queue[client].Push(pad);
        }
        if (acc[i] > 1.75f || acc[i] < -1.75f) {
            pad.motion = static_cast<PadMotion>(i + 3);
            pad.motion_value = acc[i];
            pad_queue[client].Push(pad);
        }
    }
}

void Client::BeginConfiguration() {
    for (auto& pq : pad_queue) {
        pq.Clear();
    }
    configuring = true;
}

void Client::EndConfiguration() {
    for (auto& pq : pad_queue) {
        pq.Clear();
    }
    configuring = false;
}

DeviceStatus& Client::GetPadState(std::size_t pad) {
    return clients[pad].status;
}

const DeviceStatus& Client::GetPadState(std::size_t pad) const {
    return clients[pad].status;
}

std::array<Common::SPSCQueue<UDPPadStatus>, 4>& Client::GetPadQueue() {
    return pad_queue;
}

const std::array<Common::SPSCQueue<UDPPadStatus>, 4>& Client::GetPadQueue() const {
    return pad_queue;
}

void TestCommunication(const std::string& host, u16 port, std::size_t pad_index, u32 client_id,
                       std::function<void()> success_callback,
                       std::function<void()> failure_callback) {
    std::thread([=] {
        Common::Event success_event;
        SocketCallback callback{[](Response::Version version) {}, [](Response::PortInfo info) {},
                                [&](Response::PadData data) { success_event.Set(); }};
        Socket socket{host, port, pad_index, client_id, std::move(callback)};
        std::thread worker_thread{SocketLoop, &socket};
        bool result = success_event.WaitFor(std::chrono::seconds(8));
        socket.Stop();
        worker_thread.join();
        if (result) {
            success_callback();
        } else {
            failure_callback();
        }
    }).detach();
}

CalibrationConfigurationJob::CalibrationConfigurationJob(
    const std::string& host, u16 port, std::size_t pad_index, u32 client_id,
    std::function<void(Status)> status_callback,
    std::function<void(u16, u16, u16, u16)> data_callback) {

    std::thread([=, this] {
        constexpr u16 CALIBRATION_THRESHOLD = 100;

        u16 min_x{UINT16_MAX};
        u16 min_y{UINT16_MAX};
        u16 max_x{};
        u16 max_y{};

        Status current_status{Status::Initialized};
        SocketCallback callback{[](Response::Version version) {}, [](Response::PortInfo info) {},
                                [&](Response::PadData data) {
                                    if (current_status == Status::Initialized) {
                                        // Receiving data means the communication is ready now
                                        current_status = Status::Ready;
                                        status_callback(current_status);
                                    }
                                    if (data.touch_1.is_active == 0) {
                                        return;
                                    }
                                    LOG_DEBUG(Input, "Current touch: {} {}", data.touch_1.x,
                                              data.touch_1.y);
                                    min_x = std::min(min_x, static_cast<u16>(data.touch_1.x));
                                    min_y = std::min(min_y, static_cast<u16>(data.touch_1.y));
                                    if (current_status == Status::Ready) {
                                        // First touch - min data (min_x/min_y)
                                        current_status = Status::Stage1Completed;
                                        status_callback(current_status);
                                    }
                                    if (data.touch_1.x - min_x > CALIBRATION_THRESHOLD &&
                                        data.touch_1.y - min_y > CALIBRATION_THRESHOLD) {
                                        // Set the current position as max value and finishes
                                        // configuration
                                        max_x = data.touch_1.x;
                                        max_y = data.touch_1.y;
                                        current_status = Status::Completed;
                                        data_callback(min_x, min_y, max_x, max_y);
                                        status_callback(current_status);

                                        complete_event.Set();
                                    }
                                }};
        Socket socket{host, port, pad_index, client_id, std::move(callback)};
        std::thread worker_thread{SocketLoop, &socket};
        complete_event.Wait();
        socket.Stop();
        worker_thread.join();
    }).detach();
}

CalibrationConfigurationJob::~CalibrationConfigurationJob() {
    Stop();
}

void CalibrationConfigurationJob::Stop() {
    complete_event.Set();
}

} // namespace InputCommon::CemuhookUDP
