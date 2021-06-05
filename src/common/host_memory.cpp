#ifdef _WIN32

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0A00 // Windows 10

#include <windows.h>

#include <boost/icl/separate_interval_set.hpp>

#include <iterator>
#include <unordered_map>

#pragma comment(lib, "mincore.lib")

#elif defined(__linux__) // ^^^ Windows ^^^ vvv Linux vvv

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#endif // ^^^ Linux ^^^

#include <mutex>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/host_memory.h"
#include "common/logging/log.h"
#include "common/scope_exit.h"

namespace Common {

constexpr size_t PageAlignment = 0x1000;
constexpr size_t HugePageSize = 0x200000;

#ifdef _WIN32

class HostMemory::Impl {
public:
    explicit Impl(size_t backing_size_, size_t virtual_size_)
        : backing_size{backing_size_}, virtual_size{virtual_size_}, process{GetCurrentProcess()} {
        // Allocate backing file map
        backing_handle =
            CreateFileMapping2(INVALID_HANDLE_VALUE, nullptr, FILE_MAP_WRITE | FILE_MAP_READ,
                               PAGE_READWRITE, SEC_COMMIT, backing_size, nullptr, nullptr, 0);
        if (!backing_handle) {
            throw std::bad_alloc{};
        }
        // Allocate a virtual memory for the backing file map as placeholder
        backing_base = static_cast<u8*>(VirtualAlloc2(process, nullptr, backing_size,
                                                      MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
                                                      PAGE_NOACCESS, nullptr, 0));
        if (!backing_base) {
            Release();
            throw std::bad_alloc{};
        }
        // Map backing placeholder
        void* const ret = MapViewOfFile3(backing_handle, process, backing_base, 0, backing_size,
                                         MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0);
        if (ret != backing_base) {
            Release();
            throw std::bad_alloc{};
        }
        // Allocate virtual address placeholder
        virtual_base = static_cast<u8*>(VirtualAlloc2(process, nullptr, virtual_size,
                                                      MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
                                                      PAGE_NOACCESS, nullptr, 0));
        if (!virtual_base) {
            Release();
            throw std::bad_alloc{};
        }
    }

    ~Impl() {
        Release();
    }

    void Map(size_t virtual_offset, size_t host_offset, size_t length) {
        std::unique_lock lock{placeholder_mutex};
        if (!IsNiechePlaceholder(virtual_offset, length)) {
            Split(virtual_offset, length);
        }
        ASSERT(placeholders.find({virtual_offset, virtual_offset + length}) == placeholders.end());
        TrackPlaceholder(virtual_offset, host_offset, length);

        MapView(virtual_offset, host_offset, length);
    }

    void Unmap(size_t virtual_offset, size_t length) {
        std::lock_guard lock{placeholder_mutex};

        // Unmap until there are no more placeholders
        while (UnmapOnePlaceholder(virtual_offset, length)) {
        }
    }

    void Protect(size_t virtual_offset, size_t length, bool read, bool write) {
        DWORD new_flags{};
        if (read && write) {
            new_flags = PAGE_READWRITE;
        } else if (read && !write) {
            new_flags = PAGE_READONLY;
        } else if (!read && !write) {
            new_flags = PAGE_NOACCESS;
        } else {
            UNIMPLEMENTED_MSG("Protection flag combination read={} write={}", read, write);
        }
        DWORD old_flags{};
        if (!VirtualProtect(virtual_base + virtual_offset, length, new_flags, &old_flags)) {
            LOG_CRITICAL(HW_Memory, "Failed to change virtual memory protect rules");
        }
    }

    const size_t backing_size; ///< Size of the backing memory in bytes
    const size_t virtual_size; ///< Size of the virtual address placeholder in bytes

    u8* backing_base{};
    u8* virtual_base{};

private:
    /// Release all resources in the object
    void Release() {
        if (!placeholders.empty()) {
            for (const auto& placeholder : placeholders) {
                if (!UnmapViewOfFile2(process, virtual_base + placeholder.lower(),
                                      MEM_PRESERVE_PLACEHOLDER)) {
                    LOG_CRITICAL(HW_Memory, "Failed to unmap virtual memory placeholder");
                }
            }
            Coalesce(0, virtual_size);
        }
        if (virtual_base) {
            if (!VirtualFree(virtual_base, 0, MEM_RELEASE)) {
                LOG_CRITICAL(HW_Memory, "Failed to free virtual memory");
            }
        }
        if (backing_base) {
            if (!UnmapViewOfFile2(process, backing_base, MEM_PRESERVE_PLACEHOLDER)) {
                LOG_CRITICAL(HW_Memory, "Failed to unmap backing memory placeholder");
            }
            if (!VirtualFreeEx(process, backing_base, 0, MEM_RELEASE)) {
                LOG_CRITICAL(HW_Memory, "Failed to free backing memory");
            }
        }
        if (!CloseHandle(backing_handle)) {
            LOG_CRITICAL(HW_Memory, "Failed to free backing memory file handle");
        }
    }

    /// Unmap one placeholder in the given range (partial unmaps are supported)
    /// Return true when there are no more placeholders to unmap
    bool UnmapOnePlaceholder(size_t virtual_offset, size_t length) {
        const auto it = placeholders.find({virtual_offset, virtual_offset + length});
        const auto begin = placeholders.begin();
        const auto end = placeholders.end();
        if (it == end) {
            return false;
        }
        const size_t placeholder_begin = it->lower();
        const size_t placeholder_end = it->upper();
        const size_t unmap_begin = std::max(virtual_offset, placeholder_begin);
        const size_t unmap_end = std::min(virtual_offset + length, placeholder_end);
        ASSERT(unmap_begin >= placeholder_begin && unmap_begin < placeholder_end);
        ASSERT(unmap_end <= placeholder_end && unmap_end > placeholder_begin);

        const auto host_pointer_it = placeholder_host_pointers.find(placeholder_begin);
        ASSERT(host_pointer_it != placeholder_host_pointers.end());
        const size_t host_offset = host_pointer_it->second;

        const bool split_left = unmap_begin > placeholder_begin;
        const bool split_right = unmap_end < placeholder_end;

        if (!UnmapViewOfFile2(process, virtual_base + placeholder_begin,
                              MEM_PRESERVE_PLACEHOLDER)) {
            LOG_CRITICAL(HW_Memory, "Failed to unmap placeholder");
        }
        // If we have to remap memory regions due to partial unmaps, we are in a data race as
        // Windows doesn't support remapping memory without unmapping first. Avoid adding any extra
        // logic within the panic region described below.

        // Panic region, we are in a data race right now
        if (split_left || split_right) {
            Split(unmap_begin, unmap_end - unmap_begin);
        }
        if (split_left) {
            MapView(placeholder_begin, host_offset, unmap_begin - placeholder_begin);
        }
        if (split_right) {
            MapView(unmap_end, host_offset + unmap_end - placeholder_begin,
                    placeholder_end - unmap_end);
        }
        // End panic region

        size_t coalesce_begin = unmap_begin;
        if (!split_left) {
            // Try to coalesce pages to the left
            coalesce_begin = it == begin ? 0 : std::prev(it)->upper();
            if (coalesce_begin != placeholder_begin) {
                Coalesce(coalesce_begin, unmap_end - coalesce_begin);
            }
        }
        if (!split_right) {
            // Try to coalesce pages to the right
            const auto next = std::next(it);
            const size_t next_begin = next == end ? virtual_size : next->lower();
            if (placeholder_end != next_begin) {
                // We can coalesce to the right
                Coalesce(coalesce_begin, next_begin - coalesce_begin);
            }
        }
        // Remove and reinsert placeholder trackers
        UntrackPlaceholder(it);
        if (split_left) {
            TrackPlaceholder(placeholder_begin, host_offset, unmap_begin - placeholder_begin);
        }
        if (split_right) {
            TrackPlaceholder(unmap_end, host_offset + unmap_end - placeholder_begin,
                             placeholder_end - unmap_end);
        }
        return true;
    }

    void MapView(size_t virtual_offset, size_t host_offset, size_t length) {
        if (!MapViewOfFile3(backing_handle, process, virtual_base + virtual_offset, host_offset,
                            length, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0)) {
            LOG_CRITICAL(HW_Memory, "Failed to map placeholder");
        }
    }

    void Split(size_t virtual_offset, size_t length) {
        if (!VirtualFreeEx(process, reinterpret_cast<LPVOID>(virtual_base + virtual_offset), length,
                           MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) {
            LOG_CRITICAL(HW_Memory, "Failed to split placeholder");
        }
    }

    void Coalesce(size_t virtual_offset, size_t length) {
        if (!VirtualFreeEx(process, reinterpret_cast<LPVOID>(virtual_base + virtual_offset), length,
                           MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS)) {
            LOG_CRITICAL(HW_Memory, "Failed to coalesce placeholders");
        }
    }

    void TrackPlaceholder(size_t virtual_offset, size_t host_offset, size_t length) {
        placeholders.insert({virtual_offset, virtual_offset + length});
        placeholder_host_pointers.emplace(virtual_offset, host_offset);
    }

    void UntrackPlaceholder(boost::icl::separate_interval_set<size_t>::iterator it) {
        placeholders.erase(it);
        placeholder_host_pointers.erase(it->lower());
    }

    /// Return true when a given memory region is a "nieche" and the placeholders don't have to be
    /// splitted.
    bool IsNiechePlaceholder(size_t virtual_offset, size_t length) const {
        const auto it = placeholders.upper_bound({virtual_offset, virtual_offset + length});
        if (it != placeholders.end() && it->lower() == virtual_offset + length) {
            const bool is_root = it == placeholders.begin() && virtual_offset == 0;
            return is_root || std::prev(it)->upper() == virtual_offset;
        }
        return false;
    }

    HANDLE process{};        ///< Current process handle
    HANDLE backing_handle{}; ///< File based backing memory

    std::mutex placeholder_mutex;                                 ///< Mutex for placeholders
    boost::icl::separate_interval_set<size_t> placeholders;       ///< Mapped placeholders
    std::unordered_map<size_t, size_t> placeholder_host_pointers; ///< Placeholder backing offset
};

#elif defined(__linux__) // ^^^ Windows ^^^ vvv Linux vvv

class HostMemory::Impl {
public:
    explicit Impl(size_t backing_size_, size_t virtual_size_)
        : backing_size{backing_size_}, virtual_size{virtual_size_} {
        bool good = false;
        SCOPE_EXIT({
            if (!good) {
                Release();
            }
        });

        // Backing memory initialization
        fd = memfd_create("HostMemory", 0);
        if (fd == -1) {
            LOG_CRITICAL(HW_Memory, "memfd_create failed: {}", strerror(errno));
            throw std::bad_alloc{};
        }

        // Defined to extend the file with zeros
        int ret = ftruncate(fd, backing_size);
        if (ret != 0) {
            LOG_CRITICAL(HW_Memory, "ftruncate failed with {}, are you out-of-memory?",
                         strerror(errno));
            throw std::bad_alloc{};
        }

        backing_base = static_cast<u8*>(
            mmap(nullptr, backing_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (backing_base == MAP_FAILED) {
            LOG_CRITICAL(HW_Memory, "mmap failed: {}", strerror(errno));
            throw std::bad_alloc{};
        }

        // Virtual memory initialization
        virtual_base = static_cast<u8*>(
            mmap(nullptr, virtual_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (virtual_base == MAP_FAILED) {
            LOG_CRITICAL(HW_Memory, "mmap failed: {}", strerror(errno));
            throw std::bad_alloc{};
        }

        good = true;
    }

    ~Impl() {
        Release();
    }

    void Map(size_t virtual_offset, size_t host_offset, size_t length) {

        void* ret = mmap(virtual_base + virtual_offset, length, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_FIXED, fd, host_offset);
        ASSERT_MSG(ret != MAP_FAILED, "mmap failed: {}", strerror(errno));
    }

    void Unmap(size_t virtual_offset, size_t length) {
        // The method name is wrong. We're still talking about the virtual range.
        // We don't want to unmap, we want to reserve this memory.

        void* ret = mmap(virtual_base + virtual_offset, length, PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        ASSERT_MSG(ret != MAP_FAILED, "mmap failed: {}", strerror(errno));
    }

    void Protect(size_t virtual_offset, size_t length, bool read, bool write) {
        int flags = 0;
        if (read) {
            flags |= PROT_READ;
        }
        if (write) {
            flags |= PROT_WRITE;
        }
        int ret = mprotect(virtual_base + virtual_offset, length, flags);
        ASSERT_MSG(ret == 0, "mprotect failed: {}", strerror(errno));
    }

    const size_t backing_size; ///< Size of the backing memory in bytes
    const size_t virtual_size; ///< Size of the virtual address placeholder in bytes

    u8* backing_base{reinterpret_cast<u8*>(MAP_FAILED)};
    u8* virtual_base{reinterpret_cast<u8*>(MAP_FAILED)};

private:
    /// Release all resources in the object
    void Release() {
        if (virtual_base != MAP_FAILED) {
            int ret = munmap(virtual_base, virtual_size);
            ASSERT_MSG(ret == 0, "munmap failed: {}", strerror(errno));
        }

        if (backing_base != MAP_FAILED) {
            int ret = munmap(backing_base, backing_size);
            ASSERT_MSG(ret == 0, "munmap failed: {}", strerror(errno));
        }

        if (fd != -1) {
            int ret = close(fd);
            ASSERT_MSG(ret == 0, "close failed: {}", strerror(errno));
        }
    }

    int fd{-1}; // memfd file descriptor, -1 is the error value of memfd_create
};

#else // ^^^ Linux ^^^

#error Please implement the host memory for your platform

#endif

HostMemory::HostMemory(size_t backing_size_, size_t virtual_size_)
    : backing_size(backing_size_),
      virtual_size(virtual_size_), impl{std::make_unique<HostMemory::Impl>(
                                       AlignUp(backing_size, PageAlignment),
                                       AlignUp(virtual_size, PageAlignment) + 3 * HugePageSize)},
      backing_base{impl->backing_base}, virtual_base{impl->virtual_base} {
    virtual_base += 2 * HugePageSize - 1;
    virtual_base -= reinterpret_cast<size_t>(virtual_base) & (HugePageSize - 1);
    virtual_base_offset = virtual_base - impl->virtual_base;
}

HostMemory::~HostMemory() = default;

HostMemory::HostMemory(HostMemory&&) noexcept = default;

HostMemory& HostMemory::operator=(HostMemory&&) noexcept = default;

void HostMemory::Map(size_t virtual_offset, size_t host_offset, size_t length) {
    ASSERT(virtual_offset % PageAlignment == 0);
    ASSERT(host_offset % PageAlignment == 0);
    ASSERT(length % PageAlignment == 0);
    ASSERT(virtual_offset + length <= virtual_size);
    ASSERT(host_offset + length <= backing_size);
    if (length == 0) {
        return;
    }
    impl->Map(virtual_offset + virtual_base_offset, host_offset, length);
}

void HostMemory::Unmap(size_t virtual_offset, size_t length) {
    ASSERT(virtual_offset % PageAlignment == 0);
    ASSERT(length % PageAlignment == 0);
    ASSERT(virtual_offset + length <= virtual_size);
    if (length == 0) {
        return;
    }
    impl->Unmap(virtual_offset + virtual_base_offset, length);
}

void HostMemory::Protect(size_t virtual_offset, size_t length, bool read, bool write) {
    ASSERT(virtual_offset % PageAlignment == 0);
    ASSERT(length % PageAlignment == 0);
    ASSERT(virtual_offset + length <= virtual_size);
    if (length == 0) {
        return;
    }
    impl->Protect(virtual_offset + virtual_base_offset, length, read, write);
}

} // namespace Common
