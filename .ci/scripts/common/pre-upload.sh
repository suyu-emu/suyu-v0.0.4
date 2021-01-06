#!/bin/bash -ex

GITDATE="`git show -s --date=short --format='%ad' | sed 's/-//g'`"
GITREV="`git show -s --format='%h'`"
ARTIFACTS_DIR="artifacts"

mkdir -p "${ARTIFACTS_DIR}/"
