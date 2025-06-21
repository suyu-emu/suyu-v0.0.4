// SPDX-FileCopyrightText: 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <unordered_map>
#include <vector>
#include <oaknut/code_block.hpp>
#include <oaknut/oaknut.hpp>

#include "common/common_types.h"
#include "common/settings.h"
#include "core/hle/kernel/code_set.h"
#include "core/hle/kernel/k_typed_address.h"
#include "core/hle/kernel/physical_memory.h"
#include "lru_cache.h"
#include <utility>

namespace Core::NCE {

enum class PatchMode : u32 {
    None,
    PreText,  ///< Patch section is inserted before .text
    PostData, ///< Patch section is inserted after .data
};

using ModuleTextAddress = u64;
using PatchTextAddress = u64;
using EntryTrampolines = std::unordered_map<ModuleTextAddress, PatchTextAddress>;

class Patcher {
public:
    Patcher(const Patcher&) = delete;
    Patcher& operator=(const Patcher&) = delete;
    Patcher(Patcher&& other) noexcept;
    Patcher& operator=(Patcher&&) noexcept = delete;
    explicit Patcher();
    ~Patcher();

    bool PatchText(const Kernel::PhysicalMemory& program_image,
                   const Kernel::CodeSet::Segment& code);
    bool RelocateAndCopy(Common::ProcessAddress load_base, const Kernel::CodeSet::Segment& code,
                         Kernel::PhysicalMemory& program_image, EntryTrampolines* out_trampolines);
    size_t GetSectionSize() const noexcept;

    [[nodiscard]] PatchMode GetPatchMode() const noexcept {
        return mode;
    }

private:
    using ModuleDestLabel = uintptr_t;

    struct Trampoline {
        ptrdiff_t patch_offset;
        uintptr_t module_offset;
    };

    void WriteLoadContext();
    void WriteSaveContext();
    void LockContext();
    void UnlockContext();
    void WriteSvcTrampoline(ModuleDestLabel module_dest, u32 svc_id);
    void WriteMrsHandler(ModuleDestLabel module_dest, oaknut::XReg dest_reg,
                         oaknut::SystemReg src_reg);
    void WriteMsrHandler(ModuleDestLabel module_dest, oaknut::XReg src_reg);
    void WriteCntpctHandler(ModuleDestLabel module_dest, oaknut::XReg dest_reg);

private:
    static constexpr size_t CACHE_SIZE = 16384;  // Cache size for patch entries
    LRUCache<uintptr_t, PatchTextAddress> patch_cache{CACHE_SIZE, Settings::values.lru_cache_enabled.GetValue()};

    void BranchToPatch(uintptr_t module_dest) {
        if (patch_cache.isEnabled()) {
            LOG_DEBUG(Core_ARM, "LRU cache lookup for address {:#x}", module_dest);
            // Try to get existing patch entry from cache
            if (auto* cached_patch = patch_cache.get(module_dest)) {
                LOG_WARNING(Core_ARM, "LRU cache hit for address {:#x}", module_dest);
                curr_patch->m_branch_to_patch_relocations.push_back({c.offset(), *cached_patch});
                return;
            }
            LOG_DEBUG(Core_ARM, "LRU cache miss for address {:#x}, creating new patch", module_dest);

            // If not in cache, create new entry and cache it
            const auto patch_addr = c.offset();
            curr_patch->m_branch_to_patch_relocations.push_back({patch_addr, module_dest});
            patch_cache.put(module_dest, patch_addr);
        } else {
            LOG_DEBUG(Core_ARM, "LRU cache disabled - creating direct patch for address {:#x}", module_dest);
            // LRU disabled - use pre-LRU approach
            curr_patch->m_branch_to_patch_relocations.push_back({c.offset(), module_dest});
        }
    }

    void BranchToModule(uintptr_t module_dest) {
        curr_patch->m_branch_to_module_relocations.push_back({c.offset(), module_dest});
        c.dw(0);
    }

    void WriteModulePc(uintptr_t module_dest) {
        curr_patch->m_write_module_pc_relocations.push_back({c.offset(), module_dest});
        c.dx(0);
    }

private:
    // List of patch instructions we have generated.
    std::vector<u32> m_patch_instructions{};

    // Relocation type for relative branch from module to patch.
    struct Relocation {
        ptrdiff_t patch_offset;  ///< Offset in bytes from the start of the patch section.
        uintptr_t module_offset; ///< Offset in bytes from the start of the text section.
    };

    struct ModulePatch {
        std::vector<Trampoline> m_trampolines;
        std::vector<Relocation> m_branch_to_patch_relocations{};
        std::vector<Relocation> m_branch_to_module_relocations{};
        std::vector<Relocation> m_write_module_pc_relocations{};
        std::vector<ModuleTextAddress> m_exclusives{};
    };

    oaknut::VectorCodeGenerator c;
    oaknut::Label m_save_context{};
    oaknut::Label m_load_context{};
    PatchMode mode{PatchMode::None};
    size_t total_program_size{};
    size_t m_relocate_module_index{};
    std::vector<ModulePatch> modules;
    ModulePatch* curr_patch;
};

} // namespace Core::NCE
