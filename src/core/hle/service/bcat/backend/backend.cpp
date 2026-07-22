// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/hex_util.h"
#include "common/logging.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/bcat/backend/backend.h"

namespace Service::BCAT {

ProgressServiceBackend::ProgressServiceBackend(Core::System& system, std::string_view event_name)
    : service_context{system, "ProgressServiceBackend"}
{
    update_event = service_context.CreateEvent("ProgressServiceBackend:UpdateEvent:" + std::string(event_name));
}

ProgressServiceBackend::~ProgressServiceBackend() {
    service_context.CloseEvent(update_event);
}

Kernel::KReadableEvent& ProgressServiceBackend::GetEvent() {
    return update_event->GetReadableEvent();
}

DeliveryCacheProgressImpl& ProgressServiceBackend::GetImpl() {
    return impl;
}

void ProgressServiceBackend::SetTotalSize(Kernel::KernelCore& kernel, u64 size) {
    impl.total_bytes = size;
    SignalUpdate(kernel);
}

void ProgressServiceBackend::StartConnecting(Kernel::KernelCore& kernel) {
    impl.status = DeliveryCacheProgressStatus::Connecting;
    SignalUpdate(kernel);
}

void ProgressServiceBackend::StartProcessingDataList(Kernel::KernelCore& kernel) {
    impl.status = DeliveryCacheProgressStatus::ProcessingDataList;
    SignalUpdate(kernel);
}

void ProgressServiceBackend::StartDownloadingFile(Kernel::KernelCore& kernel, std::string_view dir_name, std::string_view file_name, u64 file_size) {
    impl.status = DeliveryCacheProgressStatus::Downloading;
    impl.current_downloaded_bytes = 0;
    impl.current_total_bytes = file_size;
    std::memcpy(impl.current_directory.data(), dir_name.data(), std::min<u64>(dir_name.size(), 0x31ull));
    std::memcpy(impl.current_file.data(), file_name.data(), std::min<u64>(file_name.size(), 0x31ull));
    SignalUpdate(kernel);
}

void ProgressServiceBackend::UpdateFileProgress(Kernel::KernelCore& kernel, u64 downloaded) {
    impl.current_downloaded_bytes = downloaded;
    SignalUpdate(kernel);
}

void ProgressServiceBackend::FinishDownloadingFile(Kernel::KernelCore& kernel) {
    impl.total_downloaded_bytes += impl.current_total_bytes;
    SignalUpdate(kernel);
}

void ProgressServiceBackend::CommitDirectory(Kernel::KernelCore& kernel, std::string_view dir_name) {
    impl.status = DeliveryCacheProgressStatus::Committing;
    impl.current_file.fill(0);
    impl.current_downloaded_bytes = 0;
    impl.current_total_bytes = 0;
    std::memcpy(impl.current_directory.data(), dir_name.data(), std::min<u64>(dir_name.size(), 0x31ull));
    SignalUpdate(kernel);
}

void ProgressServiceBackend::FinishDownload(Kernel::KernelCore& kernel, Result result) {
    impl.total_downloaded_bytes = impl.total_bytes;
    impl.status = DeliveryCacheProgressStatus::Done;
    impl.result = result;
    SignalUpdate(kernel);
}

void ProgressServiceBackend::SignalUpdate(Kernel::KernelCore& kernel) {
    update_event->Signal(kernel);
}

BcatBackend::BcatBackend(DirectoryGetter getter) : dir_getter(std::move(getter)) {}
BcatBackend::~BcatBackend() = default;

NullBcatBackend::NullBcatBackend(DirectoryGetter getter) : BcatBackend(std::move(getter)) {}
NullBcatBackend::~NullBcatBackend() = default;

bool NullBcatBackend::Synchronize(Kernel::KernelCore& kernel, TitleIDVersion title, ProgressServiceBackend& progress) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, build_id={:016X}", title.title_id, title.build_id);
    progress.FinishDownload(kernel, ResultSuccess);
    return true;
}

bool NullBcatBackend::SynchronizeDirectory(Kernel::KernelCore& kernel, TitleIDVersion title, std::string name, ProgressServiceBackend& progress) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, build_id={:016X}, name={}", title.title_id, title.build_id, name);
    progress.FinishDownload(kernel, ResultSuccess);
    return true;
}

bool NullBcatBackend::Clear(Kernel::KernelCore& kernel, u64 title_id) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}", title_id);

    return true;
}

void NullBcatBackend::SetPassphrase(Kernel::KernelCore& kernel, u64 title_id, const Passphrase& passphrase) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, passphrase={}", title_id, Common::HexToString(passphrase));
}

std::optional<std::vector<u8>> NullBcatBackend::GetLaunchParameter(Kernel::KernelCore& kernel, TitleIDVersion title) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}, build_id={:016X}", title.title_id, title.build_id);
    return std::nullopt;
}

} // namespace Service::BCAT
