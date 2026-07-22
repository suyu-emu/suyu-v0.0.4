// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/elf.h"
#include "core/arm/symbols.h"
#include "core/core.h"
#include "core/memory.h"

using namespace Common::ELF;

namespace Core {
namespace Symbols {

struct ModuleHeaderLocation {
    u32 version;
    u32 header_offset;
    u32 version_offset;
};
static_assert(sizeof(ModuleHeaderLocation) == 0x0C);
struct ModuleHeader {
    u32 signature;
    u32 dynamic_offset;
    u32 bss_start_offset;
    u32 bss_end_offset;
    u32 exception_info_start_offset;
    u32 exception_info_end_offset;
    u32 module_offset;
    u32 relro_start_offset;
    u32 full_relro_end_offset;
    u32 nx_debug_link_start_offset;
    u32 nx_debug_link_end_offset;
    u32 note_gnu_build_id_start_offset;
    u32 note_gnu_build_id_end_offset;
};
static_assert(sizeof(ModuleHeader) == 0x34);

struct Mod32 {
    using Sym = Elf32_Sym;
    using Dyn = Elf32_Dyn;
};
struct Mod64 {
    using Sym = Elf64_Sym;
    using Dyn = Elf64_Dyn;
};

template <typename M, typename F>
static Symbols GetSymbols(F&& ReadBytes) {
    const auto Read8 = [&](u64 index) {
        u8 ret;
        ReadBytes(&ret, index, sizeof(u8));
        return ret;
    };
    ModuleHeaderLocation loc{};
    ReadBytes(&loc, 0, sizeof(ModuleHeaderLocation));
    ModuleHeader hdr;
    ReadBytes(&hdr, loc.header_offset, sizeof(ModuleHeader));
    if (hdr.signature != Common::MakeMagic('M', 'O', 'D', '0')) {
        return {};
    }

    VAddr strtab_offs{};
    VAddr symtab_offs{};
    u64 syment_size = sizeof(typename M::Sym);
    VAddr dynamic_offset = loc.header_offset + hdr.dynamic_offset;
    while (true) {
        typename M::Dyn dyn;
        ReadBytes(&dyn, dynamic_offset, sizeof(typename M::Dyn));
        dynamic_offset += sizeof(typename M::Dyn);
        if (dyn.d_tag == ElfDtNull) {
            break;
        }
        if (dyn.d_tag == ElfDtStrtab) {
            strtab_offs = dyn.d_un.d_ptr;
        } else if (dyn.d_tag == ElfDtSymtab) {
            symtab_offs = dyn.d_un.d_ptr;
        } else if (dyn.d_tag == ElfDtSyment) {
            syment_size = dyn.d_un.d_val;
        }
    }
    if (strtab_offs > 0 && symtab_offs > 0) {
        Symbols out;
        VAddr symbol_index = symtab_offs;
        while (symbol_index < strtab_offs) {
            typename M::Sym symbol{};
            ReadBytes(&symbol, symbol_index, sizeof(typename M::Sym));
            VAddr offs = strtab_offs + symbol.st_name;
            std::string name{};
            for (u8 c = Read8(offs); c != 0; c = Read8(++offs))
                name += char(c);
            symbol_index += syment_size;
            out[name] = std::make_pair(symbol.st_value, symbol.st_size);
        }
        return out;
    }
    return {};
}

Symbols GetSymbols(VAddr base, Core::Memory::Memory& memory, bool is_64) {
    const auto f = [base, &memory](void* ptr, size_t offset, size_t size) {
        memory.ReadBlock(base + offset, ptr, size);
    };
    return is_64 ? GetSymbols<Mod64>(f) : GetSymbols<Mod32>(f);
}

Symbols GetSymbols(std::span<const u8> data, bool is_64) {
    const auto f = [data](void* ptr, size_t offset, size_t size) {
        std::memcpy(ptr, data.data() + offset, size);
    };
    return is_64 ? GetSymbols<Mod64>(f) : GetSymbols<Mod32>(f);
}

std::optional<std::string> GetSymbolName(const Symbols& symbols, VAddr addr) {
    const auto it = std::find_if(symbols.cbegin(), symbols.cend(), [addr](const auto& e) {
        auto const [start, size] = e.second;
        auto const end = start + size;
        return addr >= start && addr < end;
    });
    return it != symbols.cend() ? std::optional<std::string>{it->first} : std::nullopt;
}

} // namespace Symbols
} // namespace Core
