// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service {

namespace FileSystem {
class FileSystemController;
} // namespace FileSystem

namespace NS {

class IApplicationManagerInterface final : public ServiceFramework<IApplicationManagerInterface> {
public:
    explicit IApplicationManagerInterface(Core::System& system_);
    ~IApplicationManagerInterface() override;

    Result GetApplicationDesiredLanguage(u8* out_desired_language, u32 supported_languages);
    Result ConvertApplicationLanguageToLanguageCode(u64* out_language_code,
                                                    u8 application_language);

private:
    void GetApplicationControlData(HLERequestContext& ctx);
    void GetApplicationDesiredLanguage(HLERequestContext& ctx);
    void ConvertApplicationLanguageToLanguageCode(HLERequestContext& ctx);
};

class IDocumentInterface final : public ServiceFramework<IDocumentInterface> {
public:
    explicit IDocumentInterface(Core::System& system_);
    ~IDocumentInterface() override;

private:
    void ResolveApplicationContentPath(HLERequestContext& ctx);
    void GetRunningApplicationProgramId(HLERequestContext& ctx);
};

class IDownloadTaskInterface final : public ServiceFramework<IDownloadTaskInterface> {
public:
    explicit IDownloadTaskInterface(Core::System& system_);
    ~IDownloadTaskInterface() override;
};

class IReadOnlyApplicationRecordInterface final
    : public ServiceFramework<IReadOnlyApplicationRecordInterface> {
public:
    explicit IReadOnlyApplicationRecordInterface(Core::System& system_);
    ~IReadOnlyApplicationRecordInterface() override;

private:
    void HasApplicationRecord(HLERequestContext& ctx);
    void IsDataCorruptedResult(HLERequestContext& ctx);
};

class IReadOnlyApplicationControlDataInterface final
    : public ServiceFramework<IReadOnlyApplicationControlDataInterface> {
public:
    explicit IReadOnlyApplicationControlDataInterface(Core::System& system_);
    ~IReadOnlyApplicationControlDataInterface() override;

private:
    void GetApplicationControlData(HLERequestContext& ctx);
};

class NS final : public ServiceFramework<NS> {
public:
    explicit NS(const char* name, Core::System& system_);
    ~NS() override;

    std::shared_ptr<IApplicationManagerInterface> GetApplicationManagerInterface() const;

private:
    template <typename T, typename... Args>
    void PushInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NS, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<T>(system);
    }

    void PushIApplicationManagerInterface(HLERequestContext& ctx) {
        LOG_DEBUG(Service_NS, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IApplicationManagerInterface>(system);
    }

    template <typename T, typename... Args>
    std::shared_ptr<T> GetInterface(Args&&... args) const {
        static_assert(std::is_base_of_v<SessionRequestHandler, T>,
                      "Not a base of ServiceFrameworkBase");

        return std::make_shared<T>(std::forward<Args>(args)...);
    }
};

void LoopProcess(Core::System& system);

} // namespace NS
} // namespace Service
