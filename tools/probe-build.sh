#!/bin/bash
# Probe build helper.
#
#   probe-build.sh "<EXTRA_DEFS>" [fast]
#
# Why this exists: compile-wsl.sh takes no define argument, and the Makefile only learns about
# MC_NETHER_PROBE / MC_PERF_PROBE through EXTRA_DEFS. Passing that on the command line through
# wsl.exe from PowerShell loses the quoting around the source path (it has spaces), so the defines and
# the cd live here instead, in a file, where neither shell can re-split them.
#
# MakefileGame uses `EXTRA_DEFS ?=`, so an exported environment variable is picked up by make. The
# build is always a full clean one: make does not track define changes, so a `fast` build after
# flipping a define silently reuses objects built with the previous one.
set -euo pipefail

cd "/mnt/c/Users/minec/Documents/MC PSP Project/Source Code V3.0.0/PSP Minecraft Source Code"

export EXTRA_DEFS="${1:-}"
export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

echo "EXTRA_DEFS=$EXTRA_DEFS"

if [ "${2:-}" = "fast" ]; then
    ./compile-wsl.sh fast
else
    ./compile-wsl.sh
fi
