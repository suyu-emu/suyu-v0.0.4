// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/assert.h"
#include <cstdlib>
#include <sys/stat.h>
#include <vector>

enum class Opcode {
#define INST(name, cute, encode) name,
#include "shader_recompiler/frontend/maxwell/maxwell.inc"
#undef INST
};

consteval std::pair<u64, u64> MaskValueFromEncoding(const char data[20]) noexcept {
    u64 mask = 0, value = 0, bit = u64(1) << 63;
    for (int i = 0; i < 20; ++i)
        switch (data[i]) {
        case '0':
            mask |= bit;
            bit >>= 1;
            break;
        case '1':
            mask |= bit;
            value |= bit;
            bit >>= 1;
            break;
        case '-':
            bit >>= 1;
            break;
        default:
            break;
        }
    return { mask, value };
}

Opcode Decode(u64 insn) {
#define INST(name, cute, encode) \
    if (auto const p = MaskValueFromEncoding(encode); (insn & p.first) == p.second) \
        return Opcode::name;
#include "shader_recompiler/frontend/maxwell/maxwell.inc"
#undef INST
    ASSERT_MSG(false, "Invalid insn 0x{:016x}", insn);
    return Opcode::NOP;
}

const char* NameOf(Opcode opcode) {
    constexpr const char* NAME_TABLE[] = {
#define INST(name, cute, encode) cute,
#include "shader_recompiler/frontend/maxwell/maxwell.inc"
#undef INST
    };
    ASSERT_MSG(size_t(opcode) < sizeof(NAME_TABLE) / sizeof(NAME_TABLE[0]), "Invalid opcode with raw value {}", int(opcode));
    return NAME_TABLE[size_t(opcode)];
}

namespace Shader::Maxwell {
std::string_view SpecialRegGetName(size_t i) {
    switch (i) {
    case 0: return "SR_LANEID";
    case 1: return "SR_CLOCK";
    case 2: return "SR_VIRTCFG";
    case 3: return "SR_VIRTID";
    case 4: return "SR_PM0";
    case 5: return "SR_PM1";
    case 6: return "SR_PM2";
    case 7: return "SR_PM3";
    case 8: return "SR_PM4";
    case 9: return "SR_PM5";
    case 10: return "SR_PM6";
    case 11: return "SR_PM7";
    case 12: return "SR_?";
    case 13: return "SR_?";
    case 14: return "SR_?";
    case 15: return "SR_ORDERING_TICKET";
    case 16: return "SR_PRIM_TYPE";
    case 17: return "SR_INVOCATION_ID";
    case 18: return "SR_Y_DIRECTION";
    case 19: return "SR_THREAD_KILL";
    case 20: return "SM_SHADER_TYPE";
    case 21: return "SR_DIRECTCBEWRITEADDRESSLOW";
    case 22: return "SR_DIRECTCBEWRITEADDRESSHIGH";
    case 23: return "SR_DIRECTCBEWRITEENABLED";
    case 24: return "SR_MACHINE_ID_0";
    case 25: return "SR_MACHINE_ID_1";
    case 26: return "SR_MACHINE_ID_2";
    case 27: return "SR_MACHINE_ID_3";
    case 28: return "SR_AFFINITY";
    case 29: return "SR_INVOCATION_INFO";
    case 30: return "SR_WSCALEFACTOR_XY";
    case 31: return "SR_WSCALEFACTOR_Z";
    case 32: return "SR_TID";
    case 33: return "SR_TID_X";
    case 34: return "SR_TID_Y";
    case 35: return "SR_TID_Z";
    case 36: return "SR_CTA_PARAM";
    case 37: return "SR_CTAID_X";
    case 38: return "SR_CTAID_Y";
    case 39: return "SR_CTAID_Z";
    case 40: return "SR_NTID";
    case 41: return "SR_CirQueueIncrMinusOne";
    case 42: return "SR_NLATC";
    case 43: return "SR_?";
    case 44: return "SR_SM_SPA_VERSION";
    case 45: return "SR_MULTIPASSSHADERINFO";
    case 46: return "SR_LWINHI";
    case 47: return "SR_SWINHI";
    case 48: return "SR_SWINLO";
    case 49: return "SR_SWINSZ";
    case 50: return "SR_SMEMSZ";
    case 51: return "SR_SMEMBANKS";
    case 52: return "SR_LWINLO";
    case 53: return "SR_LWINSZ";
    case 54: return "SR_LMEMLOSZ";
    case 55: return "SR_LMEMHIOFF";
    case 56: return "SR_EQMASK";
    case 57: return "SR_LTMASK";
    case 58: return "SR_LEMASK";
    case 59: return "SR_GTMASK";
    case 60: return "SR_GEMASK";
    case 61: return "SR_REGALLOC";
    case 62: return "SR_BARRIERALLOC";
    case 63: return "SR_?";
    case 64: return "SR_GLOBALERRORSTATUS";
    case 65: return "SR_?";
    case 66: return "SR_WARPERRORSTATUS";
    case 67: return "SR_WARPERRORSTATUSCLEAR";
    case 68: return "SR_?";
    case 69: return "SR_?";
    case 70: return "SR_?";
    case 71: return "SR_?";
    case 72: return "SR_PM_HI0";
    case 73: return "SR_PM_HI1";
    case 74: return "SR_PM_HI2";
    case 75: return "SR_PM_HI3";
    case 76: return "SR_PM_HI4";
    case 77: return "SR_PM_HI5";
    case 78: return "SR_PM_HI6";
    case 79: return "SR_PM_HI7";
    case 80: return "SR_CLOCKLO";
    case 81: return "SR_CLOCKHI";
    case 82: return "SR_GLOBALTIMERLO";
    case 83: return "SR_GLOBALTIMERHI";
    case 84: return "SR_?";
    case 85: return "SR_?";
    case 86: return "SR_?";
    case 87: return "SR_?";
    case 88: return "SR_?";
    case 89: return "SR_?";
    case 90: return "SR_?";
    case 91: return "SR_?";
    case 92: return "SR_?";
    case 93: return "SR_?";
    case 94: return "SR_?";
    case 95: return "SR_?";
    case 96: return "SR_HWTASKID";
    case 97: return "SR_CIRCULARQUEUEENTRYINDEX";
    case 98: return "SR_CIRCULARQUEUEENTRYADDRESSLOW";
    case 99: return "SR_CIRCULARQUEUEENTRYADDRESSHIGH";
    default: return "SR_??";	}
}
}
#include "generated.cpp"

int DisasReferenceImpl(int argc, char *argv[]) {
    std::vector<uint64_t> code;
    FILE *fp = fopen(argv[1], "rb");
    if (fp != NULL) {
        struct stat st;
        fstat(fileno(fp), &st);
        auto const words = (size_t(st.st_size) / sizeof(uint64_t));
        code.resize(words + 1);
        fread(code.data(), sizeof(uint64_t), words, fp);
        fclose(fp);
    }
    for (size_t i = 0; i < code.size(); ++i) {
        printf("%016lx\t%-40s\n", code[i]
            , Shader::Maxwell::DissasemblyFormat(code[i]).data()
        );
    }
    return EXIT_SUCCESS;
}

int DisasShaderRecompilerImpl(int argc, char *argv[]) {
    std::vector<u64> code;
    FILE *fp = fopen(argv[1], "rb");
    if (fp != NULL) {
        struct stat st;
        fstat(fileno(fp), &st);
        auto const words = (size_t(st.st_size) / sizeof(u64));
        code.resize(words + 1);
        fread(code.data(), sizeof(u64), words, fp);
        fclose(fp);
    }

    for (size_t i = 0; i < code.size(); ++i) {
        auto const opcode = Decode(code[i]);
        printf("%016lx\t%s\n", code[i], NameOf(opcode));
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf(
            "usage: %s [input file] [-n/i]\n"
            "Specify -n to use a disassembler that is NOT tied to the shader recompiler\n"
            "aka. a reference disassembler\n"
            , argv[0]);
        return EXIT_FAILURE;
    }

    //DumpProgram

    if (argc >= 3) {
        if (::strcmp(argv[2], "-n") == 0 || ::strcmp(argv[2], "--new") == 0) {
            return DisasReferenceImpl(argc, argv);
        }
    }
    return DisasShaderRecompilerImpl(argc, argv);
}
