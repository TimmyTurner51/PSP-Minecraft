#!/bin/bash
# Build the MineCraft Launcher Updater -- the small program that swaps the launcher's own EBOOT.PBP.
#
# Separate from build-launcher.sh because it is a separate program with its own (empty) dependency set,
# but they ship together: the launcher hands off to this file, so a card with one and not the other has
# a launcher that cannot update itself. See LauncherUpdater/main.c.
#
# Usage:  ./build-launcher-updater.sh [fast]
#   fast   build without a clean first
set -euo pipefail

export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

cd "$(dirname "$0")"

if [ ! -x "$PSPDEV/bin/psp-gcc" ]; then
  echo "PSP toolchain not found at $PSPDEV"
  echo "Expected: $HOME/pspdev from the official Ubuntu PSPDEV release."
  exit 1
fi

echo "Building the launcher updater in LauncherUpdater/"
echo

if [ "${1:-}" != "fast" ]; then
  make -C LauncherUpdater clean >/dev/null 2>&1 || true
fi

set +e
make -C LauncherUpdater
MAKEERR=$?
set -e

if [ ! -f LauncherUpdater/LAUNCHERUPDATER.PBP ]; then
  echo
  echo "Build failed: LauncherUpdater/LAUNCHERUPDATER.PBP was not produced."
  exit "${MAKEERR:-1}"
fi

echo
echo "Launcher updater: LauncherUpdater/LAUNCHERUPDATER.PBP"
echo "Install it as PSP/GAME/Minecraft/Updater/EBOOT.PBP."
