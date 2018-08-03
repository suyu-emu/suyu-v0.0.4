// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/log.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/memory.h"

namespace Kernel {

// Lists all processes that exist in the current session.
static std::vector<SharedPtr<Process>> process_list;

SharedPtr<CodeSet> CodeSet::Create(std::string name) {
    SharedPtr<CodeSet> codeset(new CodeSet);
    codeset->name = std::move(name);
    return codeset;
}

CodeSet::CodeSet() {}
CodeSet::~CodeSet() {}

u32 Process::next_process_id;

SharedPtr<Process> Process::Create(std::string&& name) {
    SharedPtr<Process> process(new Process);

    process->name = std::move(name);
    process->flags.raw = 0;
    process->flags.memory_region.Assign(MemoryRegion::APPLICATION);
    process->status = ProcessStatus::Created;
    process->program_id = 0;

    process_list.push_back(process);
    return process;
}

void Process::ParseKernelCaps(const u32* kernel_caps, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        u32 descriptor = kernel_caps[i];
        u32 type = descriptor >> 20;

        if (descriptor == 0xFFFFFFFF) {
            // Unused descriptor entry
            continue;
        } else if ((type & 0xF00) == 0xE00) { // 0x0FFF
            // Allowed interrupts list
            LOG_WARNING(Loader, "ExHeader allowed interrupts list ignored");
        } else if ((type & 0xF80) == 0xF00) { // 0x07FF
            // Allowed syscalls mask
            unsigned int index = ((descriptor >> 24) & 7) * 24;
            u32 bits = descriptor & 0xFFFFFF;

            while (bits && index < svc_access_mask.size()) {
                svc_access_mask.set(index, bits & 1);
                ++index;
                bits >>= 1;
            }
        } else if ((type & 0xFF0) == 0xFE0) { // 0x00FF
            // Handle table size
            handle_table_size = descriptor & 0x3FF;
        } else if ((type & 0xFF8) == 0xFF0) { // 0x007F
            // Misc. flags
            flags.raw = descriptor & 0xFFFF;
        } else if ((type & 0xFFE) == 0xFF8) { // 0x001F
            // Mapped memory range
            if (i + 1 >= len || ((kernel_caps[i + 1] >> 20) & 0xFFE) != 0xFF8) {
                LOG_WARNING(Loader, "Incomplete exheader memory range descriptor ignored.");
                continue;
            }
            u32 end_desc = kernel_caps[i + 1];
            ++i; // Skip over the second descriptor on the next iteration

            AddressMapping mapping;
            mapping.address = descriptor << 12;
            VAddr end_address = end_desc << 12;

            if (mapping.address < end_address) {
                mapping.size = end_address - mapping.address;
            } else {
                mapping.size = 0;
            }

            mapping.read_only = (descriptor & (1 << 20)) != 0;
            mapping.unk_flag = (end_desc & (1 << 20)) != 0;

            address_mappings.push_back(mapping);
        } else if ((type & 0xFFF) == 0xFFE) { // 0x000F
            // Mapped memory page
            AddressMapping mapping;
            mapping.address = descriptor << 12;
            mapping.size = Memory::PAGE_SIZE;
            mapping.read_only = false;
            mapping.unk_flag = false;

            address_mappings.push_back(mapping);
        } else if ((type & 0xFE0) == 0xFC0) { // 0x01FF
            // Kernel version
            kernel_version = descriptor & 0xFFFF;

            int minor = kernel_version & 0xFF;
            int major = (kernel_version >> 8) & 0xFF;
            LOG_INFO(Loader, "ExHeader kernel version: {}.{}", major, minor);
        } else {
            LOG_ERROR(Loader, "Unhandled kernel caps descriptor: 0x{:08X}", descriptor);
        }
    }
}

void Process::Run(VAddr entry_point, s32 main_thread_priority, u32 stack_size) {
    // Allocate and map the main thread stack
    // TODO(bunnei): This is heap area that should be allocated by the kernel and not mapped as part
    // of the user address space.
    vm_manager
        .MapMemoryBlock(Memory::STACK_AREA_VADDR_END - stack_size,
                        std::make_shared<std::vector<u8>>(stack_size, 0), 0, stack_size,
                        MemoryState::Mapped)
        .Unwrap();

    vm_manager.LogLayout();
    status = ProcessStatus::Running;

    Kernel::SetupMainThread(entry_point, main_thread_priority, this);
}

void Process::LoadModule(SharedPtr<CodeSet> module_, VAddr base_addr) {
    const auto MapSegment = [&](CodeSet::Segment& segment, VMAPermission permissions,
                                MemoryState memory_state) {
        auto vma = vm_manager
                       .MapMemoryBlock(segment.addr + base_addr, module_->memory, segment.offset,
                                       segment.size, memory_state)
                       .Unwrap();
        vm_manager.Reprotect(vma, permissions);
    };

    // Map CodeSet segments
    MapSegment(module_->code, VMAPermission::ReadExecute, MemoryState::CodeStatic);
    MapSegment(module_->rodata, VMAPermission::Read, MemoryState::CodeMutable);
    MapSegment(module_->data, VMAPermission::ReadWrite, MemoryState::CodeMutable);
}

ResultVal<VAddr> Process::HeapAllocate(VAddr target, u64 size, VMAPermission perms) {
    if (target < Memory::HEAP_VADDR || target + size > Memory::HEAP_VADDR_END ||
        target + size < target) {
        return ERR_INVALID_ADDRESS;
    }

    if (heap_memory == nullptr) {
        // Initialize heap
        heap_memory = std::make_shared<std::vector<u8>>();
        heap_start = heap_end = target;
    } else {
        vm_manager.UnmapRange(heap_start, heap_end - heap_start);
    }

    // If necessary, expand backing vector to cover new heap extents.
    if (target < heap_start) {
        heap_memory->insert(begin(*heap_memory), heap_start - target, 0);
        heap_start = target;
        vm_manager.RefreshMemoryBlockMappings(heap_memory.get());
    }
    if (target + size > heap_end) {
        heap_memory->insert(end(*heap_memory), (target + size) - heap_end, 0);
        heap_end = target + size;
        vm_manager.RefreshMemoryBlockMappings(heap_memory.get());
    }
    ASSERT(heap_end - heap_start == heap_memory->size());

    CASCADE_RESULT(auto vma, vm_manager.MapMemoryBlock(target, heap_memory, target - heap_start,
                                                       size, MemoryState::Heap));
    vm_manager.Reprotect(vma, perms);

    heap_used = size;

    return MakeResult<VAddr>(heap_end - size);
}

ResultCode Process::HeapFree(VAddr target, u32 size) {
    if (target < Memory::HEAP_VADDR || target + size > Memory::HEAP_VADDR_END ||
        target + size < target) {
        return ERR_INVALID_ADDRESS;
    }

    if (size == 0) {
        return RESULT_SUCCESS;
    }

    ResultCode result = vm_manager.UnmapRange(target, size);
    if (result.IsError())
        return result;

    heap_used -= size;

    return RESULT_SUCCESS;
}

ResultCode Process::MirrorMemory(VAddr dst_addr, VAddr src_addr, u64 size) {
    auto vma = vm_manager.FindVMA(src_addr);

    ASSERT_MSG(vma != vm_manager.vma_map.end(), "Invalid memory address");
    ASSERT_MSG(vma->second.backing_block, "Backing block doesn't exist for address");

    // The returned VMA might be a bigger one encompassing the desired address.
    auto vma_offset = src_addr - vma->first;
    ASSERT_MSG(vma_offset + size <= vma->second.size,
               "Shared memory exceeds bounds of mapped block");

    const std::shared_ptr<std::vector<u8>>& backing_block = vma->second.backing_block;
    size_t backing_block_offset = vma->second.offset + vma_offset;

    CASCADE_RESULT(auto new_vma,
                   vm_manager.MapMemoryBlock(dst_addr, backing_block, backing_block_offset, size,
                                             MemoryState::Mapped));
    // Protect mirror with permissions from old region
    vm_manager.Reprotect(new_vma, vma->second.permissions);
    // Remove permissions from old region
    vm_manager.Reprotect(vma, VMAPermission::None);

    return RESULT_SUCCESS;
}

ResultCode Process::UnmapMemory(VAddr dst_addr, VAddr /*src_addr*/, u64 size) {
    return vm_manager.UnmapRange(dst_addr, size);
}

Kernel::Process::Process() {}
Kernel::Process::~Process() {}

void ClearProcessList() {
    process_list.clear();
}

SharedPtr<Process> GetProcessById(u32 process_id) {
    auto itr = std::find_if(
        process_list.begin(), process_list.end(),
        [&](const SharedPtr<Process>& process) { return process->process_id == process_id; });

    if (itr == process_list.end())
        return nullptr;

    return *itr;
}

} // namespace Kernel
