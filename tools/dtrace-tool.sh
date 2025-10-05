#!/usr/local/bin/bash -ex

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Basic script to run dtrace sampling over the program (requires Flamegraph)
# Usage is either running as: ./dtrace-tool.sh pid (then input the pid of the process)
# Or just run directly with: ./dtrace-tool.sh <command>

FLAMEGRAPH_DIR=".."
fail() {
    printf '%s\n' "$1" >&2
    exit "${2-1}"
}

[ -f $FLAMEGRAPH_DIR/FlameGraph/stackcollapse.pl ] || fail 'Where is flamegraph?'
#[ which dtrace ] || fail 'Needs DTrace installed'

read -r "Sampling Hz [800]: " TRACE_CFG_HZ
if [ -z "${TRACE_CFG_HZ}" ]; then
    TRACE_CFG_HZ=800
fi

read -r "Sampling time [5] sec: " TRACE_CFG_TIME
if [ -z "${TRACE_CFG_TIME}" ]; then
    TRACE_CFG_TIME=5
fi

TRACE_FILE=dtrace-out.user_stacks
TRACE_FOLD=dtrace-out.fold
TRACE_SVG=dtrace-out.svg
ps

if [ "$1" = 'pid' ]; then
    read -r "PID: " TRACE_CFG_PID
    sudo echo 'Sudo!'
else
    if [ -f "$1" ] && [ "$1" ]; then
    	fail 'Usage: ./tools/dtrace-profile.sh <path to program>'
    fi

    printf "Executing: "
    echo "$@"
    sudo echo 'Sudo!'
    "$@" &
    TRACE_CFG_PID=$!
fi

TRACE_PROBE="profile-${TRACE_CFG_HZ} /pid == ${TRACE_CFG_PID} && arg1/ { @[ustack()] = count(); } tick-${TRACE_CFG_TIME}s { exit(0); }"

rm -- $TRACE_SVG || echo 'Skip'

sudo dtrace -x ustackframes=100 -Z -n "$TRACE_PROBE" -o $TRACE_FILE 2>/dev/null || exit

perl $FLAMEGRAPH_DIR/FlameGraph/stackcollapse.pl $TRACE_FILE > $TRACE_FOLD || exit
perl $FLAMEGRAPH_DIR/FlameGraph/flamegraph.pl $TRACE_FOLD > $TRACE_SVG || exit

sudo chmod 0666 $TRACE_FILE
rm -- $TRACE_FILE $TRACE_FOLD