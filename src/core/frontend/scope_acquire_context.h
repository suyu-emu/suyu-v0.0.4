// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Core::Frontend {

class GraphicsContext;

/// Helper class to acquire/release window context within a given scope
class ScopeAcquireContext : NonCopyable {
public:
    explicit ScopeAcquireContext(Core::Frontend::GraphicsContext& context);
    ~ScopeAcquireContext();

private:
    Core::Frontend::GraphicsContext& context;
};

} // namespace Core::Frontend
