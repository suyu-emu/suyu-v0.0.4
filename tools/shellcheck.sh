#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# fd is slightly faster on NVMe (the syntax sux though)
if command -v fd > /dev/null; then
	fd . tools -esh -x shellcheck
else
	find tools -name "*.sh" -exec shellcheck -s sh {} \;
fi