#!/bin/bash -e

# SPDX-FileCopyrightText: 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

LIBS=$(find . externals externals/nx_tzdb src/yuzu/externals externals/ffmpeg src/dynarmic/externals -maxdepth 1 -name cpmfile.json -exec jq -j 'keys_unsorted | join(" ")' {} \; -printf " ")
tools/cpm-fetch.sh $LIBS