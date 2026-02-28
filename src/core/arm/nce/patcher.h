// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <ankerl/unordered_dense.h>
#include <vector>
#include <oaknut/code_block.hpp>
#include <oaknut/oaknut.hpp>

#include "common/logging/log.h"
#include "common/common_types.h"
#include "common/settings.h"
#include "core/hle/kernel/code_set.h"
#include "core/hle/kernel/k_typed_address.h"
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
    Split,    ///< Patch sections are inserted before .text and after .data
};


using ModuleTextAddress = u64;
using PatchTextAddress = u64;
using EntryTrampolines = ankerl::unordered_dense::map<ModuleTextAddress, PatchTextAddress>;

class Patcher {
public:
    void SetModuleID(const ModuleID& id) {
        module_id = id;
    }
    explicit Patcher();
    ~Patcher();
    bool PatchText(std::span<const u8> program_image, const Kernel::CodeSet::Segment& code);
    bool RelocateAndCopy(Common::ProcessAddress load_base, const Kernel::CodeSet::Segment& code, std::vector<u8>& program_image, EntryTrampolines* out_trampolines);
    size_t GetSectionSize() const noexcept;
    size_t GetPreSectionSize() const noexcept;

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

    // Core implementations with explicit code generator
    void WriteLoadContext(oaknut::VectorCodeGenerator& code);
    void WriteSaveContext(oaknut::VectorCodeGenerator& code);
    void LockContext(oaknut::VectorCodeGenerator& code);
    void UnlockContext(oaknut::VectorCodeGenerator& code);
    void WriteSvcTrampoline(ModuleDestLabel module_dest, u32 svc_id, oaknut::VectorCodeGenerator& code, oaknut::Label& save_ctx, oaknut::Label& load_ctx);
    void WriteMrsHandler(ModuleDestLabel module_dest, oaknut::XReg dest_reg, oaknut::SystemReg src_reg, oaknut::VectorCodeGenerator& code);
    void WriteMsrHandler(ModuleDestLabel module_dest, oaknut::XReg src_reg, oaknut::VectorCodeGenerator& code);
    void WriteCntpctHandler(ModuleDestLabel module_dest, oaknut::XReg dest_reg, oaknut::VectorCodeGenerator& code);

    // Convenience wrappers using default code generator
    void WriteLoadContext() { WriteLoadContext(c); }
    void WriteSaveContext() { WriteSaveContext(c); }
    void LockContext() { LockContext(c); }
    void UnlockContext() { UnlockContext(c); }
    void WriteSvcTrampoline(ModuleDestLabel module_dest, u32 svc_id) { WriteSvcTrampoline(module_dest, svc_id, c, m_save_context, m_load_context); }
    void WriteMrsHandler(ModuleDestLabel module_dest, oaknut::XReg dest_reg, oaknut::SystemReg src_reg) { WriteMrsHandler(module_dest, dest_reg, src_reg, c); }
    void WriteMsrHandler(ModuleDestLabel module_dest, oaknut::XReg src_reg) { WriteMsrHandler(module_dest, src_reg, c); }
    void WriteCntpctHandler(ModuleDestLabel module_dest, oaknut::XReg dest_reg) { WriteCntpctHandler(module_dest, dest_reg, c); }

private:
    void BranchToPatch(uintptr_t module_dest) {
        LOG_DEBUG(Core_ARM, "Patch for offset {:#x}", module_dest);
        curr_patch->m_branch_to_patch_relocations.push_back({c.offset(), module_dest});
    }

    void BranchToPatchPre(uintptr_t module_dest) {
         curr_patch->m_branch_to_pre_patch_relocations.push_back({c_pre.offset(), module_dest});
    }

    void BranchToModule(uintptr_t module_dest) {
        curr_patch->m_branch_to_module_relocations.push_back({c.offset(), module_dest});
        c.dw(0);
    }

    void BranchToModulePre(uintptr_t module_dest) {
        curr_patch->m_branch_to_module_relocations_pre.push_back({c_pre.offset(), module_dest});
        c_pre.dw(0);
    }

    void WriteModulePc(uintptr_t module_dest) {
        curr_patch->m_write_module_pc_relocations.push_back({c.offset(), module_dest});
        c.dx(0);
    }

    void WriteModulePcPre(uintptr_t module_dest) {
        curr_patch->m_write_module_pc_relocations_pre.push_back({c_pre.offset(), module_dest});
        c_pre.dx(0);
    }

private:
    // List of patch instructions we have generated.
    std::vector<u32> m_patch_instructions{};
    std::vector<u32> m_patch_instructions_pre{};

    // Relocation type for relative branch from module to patch.
    struct Relocation {
        ptrdiff_t patch_offset;  ///< Offset in bytes from the start of the patch section.
        uintptr_t module_offset; ///< Offset in bytes from the start of the text section.
    };

    struct ModulePatch {
        std::vector<Trampoline> m_trampolines;
        std::vector<Trampoline> m_trampolines_pre;
        std::vector<Relocation> m_branch_to_patch_relocations{};
        std::vector<Relocation> m_branch_to_pre_patch_relocations{};
        std::vector<Relocation> m_branch_to_module_relocations{};
        std::vector<Relocation> m_branch_to_module_relocations_pre{};
        std::vector<Relocation> m_write_module_pc_relocations{};
        std::vector<Relocation> m_write_module_pc_relocations_pre{};
        std::vector<ModuleTextAddress> m_exclusives{};
    };

    oaknut::VectorCodeGenerator c;
    oaknut::VectorCodeGenerator c_pre;
    oaknut::Label m_save_context{};
    oaknut::Label m_load_context{};
    oaknut::Label m_save_context_pre{};
    oaknut::Label m_load_context_pre{};
    PatchMode mode{PatchMode::None};
    size_t total_program_size{};
    size_t m_relocate_module_index{};
    std::vector<ModulePatch> modules;
    ModulePatch* curr_patch;
};

} // namespace Core::NCE
