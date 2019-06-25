// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/time/time.h"

namespace Service::Time {
class SharedMemory {
public:
    SharedMemory(Core::System& system);
    ~SharedMemory();

    // Return the shared memory handle
    Kernel::SharedPtr<Kernel::SharedMemory> GetSharedMemoryHolder() const;

    // Set memory barriers in shared memory and update them
    void SetStandardSteadyClockTimepoint(const SteadyClockTimePoint& timepoint);
    void SetStandardLocalSystemClockContext(const SystemClockContext& context);
    void SetStandardNetworkSystemClockContext(const SystemClockContext& context);
    void SetStandardUserSystemClockAutomaticCorrectionEnabled(const bool enabled);

    // Pull from memory barriers in the shared memory
    SteadyClockTimePoint GetStandardSteadyClockTimepoint() const;
    SystemClockContext GetStandardLocalSystemClockContext() const;
    SystemClockContext GetStandardNetworkSystemClockContext() const;
    bool GetStandardUserSystemClockAutomaticCorrectionEnabled() const;

    // TODO(ogniK): We have to properly simulate memory barriers, how are we going to do this?
    template <typename T>
    struct MemoryBarrier {
        u32_le read_attempt{};
        T data[2]{};

        // These are not actually memory barriers at the moment as we don't have multicore and all
        // HLE is mutexed. This will need to properly be implemented when we start updating the time
        // points on threads. As of right now, we'll be updated both values synchronously and just
        // incrementing the read_attempt to indicate that we waited.
        void StoreData(T data_to_store) {
            read_attempt++;
            data[read_attempt & 1] = data_to_store;
        }

        // For reading we're just going to read the last stored value. If there was no value stored
        // it will just end up reading an empty value as intended.
        T ReadData() const {
            return data[(read_attempt - 1) & 1];
        }
    };

    // Shared memory format
    struct Format {
        MemoryBarrier<SteadyClockTimePoint> standard_steady_clock_timepoint;
        MemoryBarrier<SystemClockContext> standard_local_system_clock_context;
        MemoryBarrier<SystemClockContext> standard_network_system_clock_context;
        MemoryBarrier<u8> standard_user_system_clock_automatic_correction;
        u32_le format_version;
    };
    static_assert(sizeof(Format) == 0xd8, "Format is an invalid size");

private:
    const std::size_t SHARED_MEMORY_SIZE = 0x1000;
    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory_holder{};
    Core::System& system;
    Format* shared_memory_format;
};

} // namespace Service::Time
