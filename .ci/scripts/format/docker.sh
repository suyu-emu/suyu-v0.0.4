#!/bin/bash -ex

# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
# 
# Modified by AMA25 on 3/5/24

# Run clang-format
cd /suyu
chmod a+x ./.ci/scripts/format/script.sh
./.ci/scripts/format/script.sh
