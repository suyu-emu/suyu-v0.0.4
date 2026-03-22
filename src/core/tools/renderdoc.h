// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

struct RENDERDOC_API_1_7_0;

namespace Tools {

class RenderdocAPI {
public:
    explicit RenderdocAPI();
    ~RenderdocAPI();

    void ToggleCapture();

private:
    RENDERDOC_API_1_7_0* rdoc_api{};
    bool is_capturing{false};
};

} // namespace Tools
