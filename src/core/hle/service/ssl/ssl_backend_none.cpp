// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ssl/ssl_backend.h"

#include "common/logging/log.h"

namespace Service::SSL {

ResultVal<std::unique_ptr<SSLConnectionBackend>> CreateSSLConnectionBackend() {
    LOG_ERROR(Service_SSL, "No SSL backend on this platform");
    return ResultInternalError;
}

} // namespace Service::SSL
