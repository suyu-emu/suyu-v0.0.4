#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# do NOT use fd in scripts, PLEASE
find tools -name "*.sh" -exec shellcheck -s sh {} \;
