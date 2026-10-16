#!/bin/bash
# Build the MineCraft Launcher -- the EBOOT the XMB runs -- and its updater helper.
#
# Separate from compile-wsl.sh, which builds the game. The three are independent programs that share
# sources (see UpdaterApp/Makefile and LauncherUpdater/Makefile), and they write different output names
# (EBOOT.PBP, LAUNCHER.PBP, LAUNCHERUPDATER.PBP) so that building one cannot clobber another's artifact.
#
# The LauncherUpdater is built here rather than in its own script entry point because the two ship
# together: the launcher hands off to that file to replace its own EBOOT, so a card with the launcher
# and not the helper has a launcher that cannot update itself. Building both from one command is how
# that pairing is hard to get wrong. (Its own script, build-launcher-updater.sh, still exists for when
# only that half needs rebuilding.)
#
# Usage:  ./build-launcher.sh [fast]
#   fast   build without a clean first
#
# NOTE ON LINE ENDINGS: this file must keep LF endings. It was CRLF for a while and that is not
# cosmetic -- `set -o pipefail` arrives as `pipefail\r` and the script dies on line 10 with
# "invalid option name" before it runs a single make, which reads as the build being broken rather
# than the file being checked out wrong.
set -euo pipefail

export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

cd "$(dirname "$0")"

if [ ! -x "$PSPDEV/bin/psp-gcc" ]; then
  echo "PSP toolchain not found at $PSPDEV"
  echo "Expected: $HOME/pspdev from the official Ubuntu PSPDEV release."
  exit 1
fi

echo "Building the launcher in UpdaterApp/"
echo

if [ "${1:-}" != "fast" ]; then
  make -C UpdaterApp clean >/dev/null 2>&1 || true
fi

set +e
make -C UpdaterApp
MAKEERR=$?
set -e

if [ ! -f UpdaterApp/LAUNCHER.PBP ]; then
  echo
  echo "Build failed: UpdaterApp/LAUNCHER.PBP was not produced."
  exit "${MAKEERR:-1}"
fi

echo
echo "Launcher: UpdaterApp/LAUNCHER.PBP"
echo "Install it as PSP/GAME/Minecraft/EBOOT.PBP."
echo

# The helper. Not fatal on its own: the launcher is still usable without it, it just cannot update
# itself, and saying so loudly is better than failing a build that produced a working launcher.
echo "Building the launcher updater in LauncherUpdater/"
echo

if [ "${1:-}" != "fast" ]; then
  make -C LauncherUpdater clean >/dev/null 2>&1 || true
fi

set +e
make -C LauncherUpdater
HELPERERR=$?
set -e

if [ ! -f LauncherUpdater/LAUNCHERUPDATER.PBP ]; then
  echo
  echo "WARNING: LauncherUpdater/LAUNCHERUPDATER.PBP was not produced (make exit ${HELPERERR:-1})."
  echo "The launcher is built, but it will refuse a launcher self-update without this file."
  exit "${HELPERERR:-1}"
fi

echo
echo "Launcher updater: LauncherUpdater/LAUNCHERUPDATER.PBP"
echo "Install it as PSP/GAME/Minecraft/Updater/EBOOT.PBP."
