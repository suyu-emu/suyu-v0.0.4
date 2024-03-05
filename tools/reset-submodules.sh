#!/bin/bash -ex

# SPDX-FileCopyrightText: 2024 Emulator Project
# SPDX-License-Identifier: MIT
#
# Modified by AMA25 on 3/5/24

git submodule sync
git submodule foreach --recursive git reset --hard
git submodule update --init --recursive
