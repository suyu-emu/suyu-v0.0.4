# SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

project(gamemode)

add_library(gamemode include/gamemode_client.h)
set_target_properties(gamemode PROPERTIES LINKER_LANGUAGE C)
