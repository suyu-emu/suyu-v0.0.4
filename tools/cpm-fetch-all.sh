#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# provided for workflow compat

# shellcheck disable=SC1091
. tools/cpm/common.sh

chmod +x tools/cpm/fetch.sh

# shellcheck disable=SC2086
tools/cpm/fetch.sh $LIBS
