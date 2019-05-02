// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/hex_util.h"
#include "common/logging/log.h"
#include "core/hle/service/bcat/backend/backend.h"

namespace Service::BCAT {

Backend::Backend(DirectoryGetter getter) : dir_getter(std::move(getter)) {}

Backend::~Backend() = default;

NullBackend::NullBackend(const DirectoryGetter& getter) : Backend(std::move(getter)) {}

NullBackend::~NullBackend() = default;

bool NullBackend::Synchronize(TitleIDVersion title, CompletionCallback callback) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, build_id={:016X}", title.title_id,
              title.build_id);

    callback(true);
    return true;
}

bool NullBackend::SynchronizeDirectory(TitleIDVersion title, std::string name,
                                       CompletionCallback callback) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, build_id={:016X}, name={}", title.title_id,
              title.build_id, name);

    callback(true);
    return true;
}

bool NullBackend::Clear(u64 title_id) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}");

    return true;
}

void NullBackend::SetPassphrase(u64 title_id, const Passphrase& passphrase) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, passphrase = {}", title_id,
              Common::HexArrayToString(passphrase));
}

std::optional<std::vector<u8>> NullBackend::GetLaunchParameter(TitleIDVersion title) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, build_id={:016X}", title.title_id,
              title.build_id);
    return std::nullopt;
}

} // namespace Service::BCAT
