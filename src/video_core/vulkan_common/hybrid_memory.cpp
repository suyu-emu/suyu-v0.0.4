// SPDX-FileCopyrightText: Copyright 2025 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <fstream>
#include <algorithm>

#include "common/logging/log.h"
#include "video_core/vulkan_common/hybrid_memory.h"

#if defined(__linux__) || defined(__ANDROID__)
#include <sys/mman.h>
#include <unistd.h>
#include <poll.h>
#include <sys/syscall.h>
#include <linux/userfaultfd.h>
#include <sys/ioctl.h>
#endif

namespace Vulkan {

void PredictiveReuseManager::RecordUsage(u64 address, u64 size, bool write_access) {
    std::lock_guard<std::mutex> guard(mutex);

    // Add to history, removing oldest entries if we're past max_history
    access_history.push_back({address, size, write_access, current_timestamp++});
    if (access_history.size() > max_history) {
        access_history.erase(access_history.begin());
    }
}

bool PredictiveReuseManager::IsHotRegion(u64 address, u64 size) const {
    std::lock_guard<std::mutex> guard(mutex);

    // Check if this memory region has been accessed frequently
    const u64 end_address = address + size;
    int access_count = 0;

    for (const auto& access : access_history) {
        const u64 access_end = access.address + access.size;

        // Check for overlap
        if (!(end_address <= access.address || address >= access_end)) {
            access_count++;
        }
    }

    // Consider a region "hot" if it has been accessed in at least 10% of recent accesses
    return access_count >= static_cast<int>(std::max<size_t>(1, max_history / 10));
}

void PredictiveReuseManager::EvictRegion(u64 address, u64 size) {
    std::lock_guard<std::mutex> guard(mutex);

    // Remove any history entries that overlap with this region
    const u64 end_address = address + size;

    access_history.erase(
        std::remove_if(access_history.begin(), access_history.end(),
            [address, end_address](const MemoryAccess& access) {
                const u64 access_end = access.address + access.size;
                // Check for overlap
                return !(end_address <= access.address || address >= access_end);
            }),
        access_history.end()
    );
}

void PredictiveReuseManager::ClearHistory() {
    std::lock_guard<std::mutex> guard(mutex);
    access_history.clear();
    current_timestamp = 0;
}

#if defined(__linux__) || defined(__ANDROID__)
void FaultManagedAllocator::Initialize(void* base, size_t size) {
    uffd = syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd < 0) {
        LOG_ERROR(Render_Vulkan, "Failed to create userfaultfd, fault handling disabled");
        return;
    }

    struct uffdio_api api = { .api = UFFD_API };
    ioctl(uffd, UFFDIO_API, &api);

    struct uffdio_register reg = {
        .range = { .start = (uintptr_t)base, .len = size },
        .mode = UFFDIO_REGISTER_MODE_MISSING
    };

    if (ioctl(uffd, UFFDIO_REGISTER, &reg) < 0) {
        LOG_ERROR(Render_Vulkan, "Failed to register memory range with userfaultfd");
        close(uffd);
        uffd = -1;
        return;
    }

    running = true;
    fault_handler = std::thread(&FaultManagedAllocator::FaultThread, this);
}

void FaultManagedAllocator::Touch(size_t addr) {
    lru.remove(addr);
    lru.push_front(addr);
    dirty_set.insert(addr);
}

void FaultManagedAllocator::EnforceLimit() {
    while (lru.size() > MaxPages) {
        size_t evict = lru.back();
        lru.pop_back();

        auto it = page_map.find(evict);
        if (it != page_map.end()) {
            if (dirty_set.count(evict)) {
                // Compress and store dirty page before evicting
                std::vector<u8> compressed((u8*)it->second, (u8*)it->second + PageSize);
                compressed_store[evict] = std::move(compressed);
                dirty_set.erase(evict);
            }

            munmap(it->second, PageSize);
            page_map.erase(it);
        }
    }
}

void* FaultManagedAllocator::GetOrAlloc(size_t addr) {
    std::lock_guard<std::mutex> guard(lock);

    if (page_map.count(addr)) {
        Touch(addr);
        return page_map[addr];
    }

    void* mem = mmap(nullptr, PageSize, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem == MAP_FAILED) {
        LOG_ERROR(Render_Vulkan, "Failed to mmap memory for fault handler");
        return nullptr;
    }

    if (compressed_store.count(addr)) {
        // Decompress stored page data
        std::memcpy(mem, compressed_store[addr].data(), compressed_store[addr].size());
        compressed_store.erase(addr);
    } else {
        std::memset(mem, 0, PageSize);
    }

    page_map[addr] = mem;
    lru.push_front(addr);
    dirty_set.insert(addr);
    EnforceLimit();

    return mem;
}

void FaultManagedAllocator::FaultThread() {
    struct pollfd pfd = { uffd, POLLIN, 0 };

    while (running) {
        if (poll(&pfd, 1, 10) > 0) {
            struct uffd_msg msg;
            read(uffd, &msg, sizeof(msg));

            if (msg.event == UFFD_EVENT_PAGEFAULT) {
                size_t addr = msg.arg.pagefault.address & ~(PageSize - 1);
                void* page = GetOrAlloc(addr);

                if (page) {
                    struct uffdio_copy copy = {
                        .src = (uintptr_t)page,
                        .dst = (uintptr_t)addr,
                        .len = PageSize,
                        .mode = 0
                    };

                    ioctl(uffd, UFFDIO_COPY, &copy);
                }
            }
        }
    }
}

void* FaultManagedAllocator::Translate(size_t addr) {
    std::lock_guard<std::mutex> guard(lock);

    size_t base = addr & ~(PageSize - 1);
    if (!page_map.count(base)) {
        return nullptr;
    }

    Touch(base);
    return (u8*)page_map[base] + (addr % PageSize);
}

void FaultManagedAllocator::SaveSnapshot(const std::string& path) {
    std::lock_guard<std::mutex> guard(lock);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        LOG_ERROR(Render_Vulkan, "Failed to open snapshot file for writing: {}", path);
        return;
    }

    for (auto& [addr, mem] : page_map) {
        out.write(reinterpret_cast<const char*>(&addr), sizeof(addr));
        out.write(reinterpret_cast<const char*>(mem), PageSize);
    }

    LOG_INFO(Render_Vulkan, "Saved memory snapshot to {}", path);
}

void FaultManagedAllocator::SaveDifferentialSnapshot(const std::string& path) {
    std::lock_guard<std::mutex> guard(lock);

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        LOG_ERROR(Render_Vulkan, "Failed to open diff snapshot file for writing: {}", path);
        return;
    }

    size_t dirty_count = 0;
    for (const auto& addr : dirty_set) {
        if (page_map.count(addr)) {
            out.write(reinterpret_cast<const char*>(&addr), sizeof(addr));
            out.write(reinterpret_cast<const char*>(page_map[addr]), PageSize);
            dirty_count++;
        }
    }

    LOG_INFO(Render_Vulkan, "Saved differential snapshot to {} ({} dirty pages)",
             path, dirty_count);
}

void FaultManagedAllocator::ClearDirtySet() {
    std::lock_guard<std::mutex> guard(lock);
    dirty_set.clear();
    LOG_DEBUG(Render_Vulkan, "Cleared dirty page tracking");
}

FaultManagedAllocator::~FaultManagedAllocator() {
    running = false;

    if (fault_handler.joinable()) {
        fault_handler.join();
    }

    for (auto& [addr, mem] : page_map) {
        munmap(mem, PageSize);
    }

    if (uffd != -1) {
        close(uffd);
    }
}
#endif // defined(__linux__) || defined(__ANDROID__)

HybridMemory::HybridMemory(const Device& device_, MemoryAllocator& allocator, size_t reuse_history)
    : device(device_), memory_allocator(allocator), reuse_manager(reuse_history) {
}

HybridMemory::~HybridMemory() = default;

void HybridMemory::InitializeGuestMemory(void* base, size_t size) {
#if defined(__linux__) || defined(__ANDROID__)
    fmaa.Initialize(base, size);
    LOG_INFO(Render_Vulkan, "Initialized fault-managed guest memory at {:p}, size: {}",
             base, size);
#else
    LOG_INFO(Render_Vulkan, "Fault-managed memory not supported on this platform");
#endif
}

void* HybridMemory::TranslateAddress(size_t addr) {
#if defined(__linux__) || defined(__ANDROID__)
    return fmaa.Translate(addr);
#else
    return nullptr;
#endif
}

ComputeBuffer HybridMemory::CreateComputeBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                               MemoryUsage memory_type) {
    ComputeBuffer buffer;
    buffer.size = size;

    VkBufferCreateInfo buffer_ci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    // Using CreateBuffer directly handles memory allocation internally
    buffer.buffer = memory_allocator.CreateBuffer(buffer_ci, memory_type);

    LOG_DEBUG(Render_Vulkan, "Created compute buffer: size={}, usage={:x}",
              size, usage);

    return buffer;
}

void HybridMemory::SaveSnapshot(const std::string& path) {
#if defined(__linux__) || defined(__ANDROID__)
    fmaa.SaveSnapshot(path);
#else
    LOG_ERROR(Render_Vulkan, "Memory snapshots not supported on this platform");
#endif
}

void HybridMemory::SaveDifferentialSnapshot(const std::string& path) {
#if defined(__linux__) || defined(__ANDROID__)
    fmaa.SaveDifferentialSnapshot(path);
#else
    LOG_ERROR(Render_Vulkan, "Differential memory snapshots not supported on this platform");
#endif
}

void HybridMemory::ResetDirtyTracking() {
#if defined(__linux__) || defined(__ANDROID__)
    fmaa.ClearDirtySet();
#endif
}

} // namespace Vulkan