// SPDX-FileCopyrightText: 2022 yuzu Emulator Project
// SPDX-FileCopyrightText: 2022 Skyline Team and Contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <atomic>
#include <deque>
#include <mutex>

#include "core/hle/kernel/k_process.h"
#include "core/hle/service/nvdrv/core/container.h"
#include "core/hle/service/nvdrv/core/nvmap.h"
#include "core/hle/service/nvdrv/core/syncpoint_manager.h"
#include "core/memory.h"
#include "video_core/host1x/host1x.h"

namespace Service::Nvidia::NvCore {

struct ContainerImpl {
    explicit ContainerImpl(Container& core, Tegra::Host1x::Host1x& host1x_)
        : host1x{host1x_}, file{core, host1x_}, manager{host1x_}, device_file_data{} {}
    Tegra::Host1x::Host1x& host1x;
    NvMap file;
    SyncpointManager manager;
    Container::Host1xDeviceFileData device_file_data;
    std::deque<Session> sessions;
    size_t new_ids{};
    std::deque<size_t> id_pool;
    std::mutex session_guard;
};

Container::Container(Tegra::Host1x::Host1x& host1x_) {
    impl = std::make_unique<ContainerImpl>(*this, host1x_);
}

Container::~Container() = default;

size_t Container::OpenSession(Kernel::KProcess* process) {
    std::scoped_lock lk(impl->session_guard);
    size_t new_id{};
    auto* memory_interface = &process->GetMemory();
    auto& smmu = impl->host1x.MemoryManager();
    auto smmu_id = smmu.RegisterProcess(memory_interface);
    if (!impl->id_pool.empty()) {
        new_id = impl->id_pool.front();
        impl->id_pool.pop_front();
        impl->sessions[new_id] = Session{new_id, process, smmu_id};
    } else {
        impl->sessions.emplace_back(new_id, process, smmu_id);
        new_id = impl->new_ids++;
    }
    LOG_CRITICAL(Debug, "Created Session {}", new_id);
    return new_id;
}

void Container::CloseSession(size_t id) {
    std::scoped_lock lk(impl->session_guard);
    auto& smmu = impl->host1x.MemoryManager();
    smmu.UnregisterProcess(impl->sessions[id].smmu_id);
    impl->id_pool.emplace_front(id);
    LOG_CRITICAL(Debug, "Closed Session {}", id);
}

Session* Container::GetSession(size_t id) {
    std::atomic_thread_fence(std::memory_order_acquire);
    return &impl->sessions[id];
}

NvMap& Container::GetNvMapFile() {
    return impl->file;
}

const NvMap& Container::GetNvMapFile() const {
    return impl->file;
}

Container::Host1xDeviceFileData& Container::Host1xDeviceFile() {
    return impl->device_file_data;
}

const Container::Host1xDeviceFileData& Container::Host1xDeviceFile() const {
    return impl->device_file_data;
}

SyncpointManager& Container::GetSyncpointManager() {
    return impl->manager;
}

const SyncpointManager& Container::GetSyncpointManager() const {
    return impl->manager;
}

} // namespace Service::Nvidia::NvCore
