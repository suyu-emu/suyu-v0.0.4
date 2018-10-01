// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "core/file_sys/vfs.h"

namespace FileSys {

VirtualFile PatchIPS(const VirtualFile& in, const VirtualFile& ips);

class IPSwitchCompiler {
public:
    explicit IPSwitchCompiler(VirtualFile patch_text);
    std::array<u8, 0x20> GetBuildID() const;
    bool IsValid() const;
    VirtualFile Apply(const VirtualFile& in) const;

private:
    void Parse();

    bool valid;

    struct IPSwitchPatch {
        std::string name;
        bool enabled;
        std::map<u32, std::vector<u8>> records;
    };

    VirtualFile patch_text;
    std::vector<IPSwitchPatch> patches;
    std::array<u8, 0x20> nso_build_id;
    bool is_little_endian;
    s64 offset_shift;
    bool print_values;
    std::string last_comment;
};

} // namespace FileSys
