#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

_svg=dev.eden_emu.eden.svg
_icon=dist/eden.icon
_composed="$_icon/Assets/$_svg"
_svg="dist/$_svg"

rm "$_composed"
cp "$_svg" "$_composed"

xcrun actool "$_icon" \
    --compile dist \
    --platform macosx \
    --minimum-deployment-target 11.0 \
    --app-icon eden \
    --output-partial-info-plist /dev/null
