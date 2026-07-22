// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdio>
#include <cstdlib>
#include <cstring>

int SpirvReferenceImpl(int argc, char *argv[]);
int SpirvShaderRecompilerImpl(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage: %s [input file]\n"
            "Specify -n to use a recompiler that is NOT tied to the shader recompiler\n"
            "aka. a reference recompiler\n"
            "RAW SPIRV CODE WILL BE SENT TO STDOUT!\n", argv[0]);
        return EXIT_FAILURE;
    }
    if (argc >= 3) {
        if (::strcmp(argv[2], "-n") == 0
        || ::strcmp(argv[2], "--new") == 0) {
            return SpirvReferenceImpl(argc, argv);
        }
    }
    return SpirvShaderRecompilerImpl(argc, argv);
}
