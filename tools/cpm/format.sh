#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

jq --indent 4 -S <cpmfile.json >cpmfile.json.new
mv cpmfile.json.new cpmfile.json

# TODO: Run some sanity checks e.g. patches exist, etc.
