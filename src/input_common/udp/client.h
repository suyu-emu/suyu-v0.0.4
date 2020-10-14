// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include "common/common_types.h"
#include "common/param_package.h"
#include "common/thread.h"
#include "common/threadsafe_queue.h"
#include "common/vector_math.h"
#include "core/frontend/input.h"
#include "input_common/motion_input.h"

namespace InputCommon::CemuhookUDP {

constexpr u16 DEFAULT_PORT = 26760;
constexpr char DEFAULT_ADDR[] = "127.0.0.1";

class Socket;

namespace Response {
struct PadData;
struct PortInfo;
struct Version;
} // namespace Response

enum class PadMotion {
    GyroX,
    GyroY,
    GyroZ,
    AccX,
    AccY,
    AccZ,
    Undefined,
};

enum class PadTouch {
    Click,
    Undefined,
};

struct UDPPadStatus {
    PadTouch touch{PadTouch::Undefined};
    PadMotion motion{PadMotion::Undefined};
    f32 motion_value{0.0f};
};

struct DeviceStatus {
    std::mutex update_mutex;
    Input::MotionStatus motion_status;
    std::tuple<float, float, bool> touch_status;

    // calibration data for scaling the device's touch area to 3ds
    struct CalibrationData {
        u16 min_x{};
        u16 min_y{};
        u16 max_x{};
        u16 max_y{};
    };
    std::optional<CalibrationData> touch_calibration;
};

class Client {
public:
    // Initialize the UDP client capture and read sequence
    Client();

    // Close and release the client
    ~Client();

    // Used for polling
    void BeginConfiguration();
    void EndConfiguration();

    std::vector<Common::ParamPackage> GetInputDevices() const;

    bool DeviceConnected(std::size_t pad) const;
    void ReloadUDPClient();
    void ReloadSocket(const std::string& host = "127.0.0.1", u16 port = 26760,
                      std::size_t pad_index = 0, u32 client_id = 24872);

    std::array<Common::SPSCQueue<UDPPadStatus>, 4>& GetPadQueue();
    const std::array<Common::SPSCQueue<UDPPadStatus>, 4>& GetPadQueue() const;

    DeviceStatus& GetPadState(std::size_t pad);
    const DeviceStatus& GetPadState(std::size_t pad) const;

private:
    struct ClientData {
        std::unique_ptr<Socket> socket;
        DeviceStatus status;
        std::thread thread;
        u64 packet_sequence = 0;
        u8 active = 0;

        // Realtime values
        // motion is initalized with PID values for drift correction on joycons
        InputCommon::MotionInput motion{0.3f, 0.005f, 0.0f};
        std::chrono::time_point<std::chrono::system_clock> last_motion_update;
    };

    // For shutting down, clear all data, join all threads, release usb
    void Reset();

    void OnVersion(Response::Version);
    void OnPortInfo(Response::PortInfo);
    void OnPadData(Response::PadData);
    void StartCommunication(std::size_t client, const std::string& host, u16 port,
                            std::size_t pad_index, u32 client_id);
    void UpdateYuzuSettings(std::size_t client, const Common::Vec3<float>& acc,
                            const Common::Vec3<float>& gyro, bool touch);

    bool configuring = false;

    std::array<ClientData, 4> clients;
    std::array<Common::SPSCQueue<UDPPadStatus>, 4> pad_queue;
};

/// An async job allowing configuration of the touchpad calibration.
class CalibrationConfigurationJob {
public:
    enum class Status {
        Initialized,
        Ready,
        Stage1Completed,
        Completed,
    };
    /**
     * Constructs and starts the job with the specified parameter.
     *
     * @param status_callback Callback for job status updates
     * @param data_callback Called when calibration data is ready
     */
    explicit CalibrationConfigurationJob(const std::string& host, u16 port, std::size_t pad_index,
                                         u32 client_id, std::function<void(Status)> status_callback,
                                         std::function<void(u16, u16, u16, u16)> data_callback);
    ~CalibrationConfigurationJob();
    void Stop();

private:
    Common::Event complete_event;
};

void TestCommunication(const std::string& host, u16 port, std::size_t pad_index, u32 client_id,
                       std::function<void()> success_callback,
                       std::function<void()> failure_callback);

} // namespace InputCommon::CemuhookUDP
