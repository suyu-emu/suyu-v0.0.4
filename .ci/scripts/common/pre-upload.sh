#!/bin/bash -ex

# SPDX-FileCopyrightText: 2024 suyu Emulator Project
# SPDX-License-Identifier: GPL-2.0-or-later
# 
# Modified by AMA25 on 3/5/24

GITDATE="`git show -s --date=short --format='%ad' | sed 's/-//g'`"
GITREV="`git show -s --format='%h'`"
ARTIFACTS_DIR="$PWD/artifacts"

mkdir -p "${ARTIFACTS_DIR}/"
