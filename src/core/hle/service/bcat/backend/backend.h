// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <optional>
#include "common/common_types.h"
#include "core/file_sys/vfs_types.h"

namespace Service::BCAT {

using CompletionCallback = std::function<void(bool)>;
using DirectoryGetter = std::function<FileSys::VirtualDir(u64)>;
using Passphrase = std::array<u8, 0x20>;

struct TitleIDVersion {
    u64 title_id;
    u64 build_id;
};

class Backend {
public:
    explicit Backend(DirectoryGetter getter);
    virtual ~Backend();

    virtual bool Synchronize(TitleIDVersion title, CompletionCallback callback) = 0;
    virtual bool SynchronizeDirectory(TitleIDVersion title, std::string name,
                                      CompletionCallback callback) = 0;

    virtual bool Clear(u64 title_id) = 0;

    virtual void SetPassphrase(u64 title_id, const Passphrase& passphrase) = 0;

    virtual std::optional<std::vector<u8>> GetLaunchParameter(TitleIDVersion title) = 0;

protected:
    DirectoryGetter dir_getter;
};

class NullBackend : public Backend {
public:
    explicit NullBackend(const DirectoryGetter& getter);
    ~NullBackend() override;

    bool Synchronize(TitleIDVersion title, CompletionCallback callback) override;
    bool SynchronizeDirectory(TitleIDVersion title, std::string name,
                              CompletionCallback callback) override;

    bool Clear(u64 title_id) override;

    void SetPassphrase(u64 title_id, const Passphrase& passphrase) override;

    std::optional<std::vector<u8>> GetLaunchParameter(TitleIDVersion title) override;
};

std::unique_ptr<Backend> CreateBackendFromSettings(DirectoryGetter getter);

} // namespace Service::BCAT
