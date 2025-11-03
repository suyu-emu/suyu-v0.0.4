// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
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
using ModuleID = std::array<u8, 32>;  // NSO build ID
struct PatchCacheKey {
    ModuleID module_id;
    uintptr_t offset;
    bool operator==(const PatchCacheKey&) const = default;
};

template <>
struct std::hash<PatchCacheKey> {
    size_t operator()(const PatchCacheKey& key) const {
        // Simple XOR hash of first few bytes
        size_t hash_ = 0;
        for (size_t i = 0; i < key.module_id.size(); ++i) {
            hash_ ^= static_cast<size_t>(key.module_id[i]) << ((i % sizeof(size_t)) * 8);
        }
        return hash_ ^ std::hash<uintptr_t>{}(key.offset);
    }
};

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
    void SetModuleID(const ModuleID& id) {
        module_id = id;
    }
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
    ModuleID module_id{};
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
    LRUCache<PatchCacheKey, PatchTextAddress> patch_cache{CACHE_SIZE, Settings::values.lru_cache_enabled.GetValue()};

    void BranchToPatch(uintptr_t module_dest) {
        if (patch_cache.isEnabled()) {
            PatchCacheKey key{module_id, module_dest};
            LOG_DEBUG(Core_ARM, "LRU cache lookup for module={}, offset={:#x}", fmt::ptr(module_id.data()), module_dest);
            // Try to get existing patch entry from cache
            if (auto* cached_patch = patch_cache.get(key)) {
                LOG_WARNING(Core_ARM, "LRU cache hit for module offset {:#x}", module_dest);
                curr_patch->m_branch_to_patch_relocations.push_back({c.offset(), *cached_patch});
                return;
            }
            LOG_DEBUG(Core_ARM, "LRU cache miss for module offset {:#x}, creating new patch", module_dest);
            // Not in cache: create and store
            const auto patch_addr = c.offset();
            curr_patch->m_branch_to_patch_relocations.push_back({patch_addr, module_dest});
            patch_cache.put(key, patch_addr);
        } else {
            LOG_DEBUG(Core_ARM, "LRU cache disabled - direct patch for offset {:#x}", module_dest);
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
