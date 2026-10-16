#!/bin/bash
set -euo pipefail

export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

cd "$(dirname "$0")"

if [ ! -x "$PSPDEV/bin/psp-gcc" ]; then
  echo "PSP toolchain not found at $PSPDEV"
  echo "Expected: $HOME/pspdev from the official Ubuntu PSPDEV release."
  exit 1
fi

echo "PSPDEV=$PSPDEV"
echo "psp-config: $(psp-config --pspdev-path)"
echo "PSPSDK:    $(psp-config --pspsdk-path)"
psp-gcc --version | head -n 1
echo "Building in $(pwd)"
echo

if [ "${1:-}" = "check" ]; then
  echo "Toolchain and project path look good."
  exit 0
fi

if [ "${1:-}" = "clean" ]; then
  make clean
  exit 0
fi

if [ "${1:-}" != "fast" ]; then
  make clean
fi

set +e
make
MAKEERR=$?
set -e

if [ "$MAKEERR" != "0" ]; then
  # pack-pbp may fail if EBOOT.PBP is locked by PPSSPP even when the PRX built.
  if [ ! -f Tutorial1.prx ]; then
    echo "Build failed."
    exit "$MAKEERR"
  fi
  echo "make exited $MAKEERR (often EBOOT.PBP locked). Tutorial1.prx exists — will pack/copy when unlocked."
fi

rm -f EBOOT_new.PBP EBOOT_NEW.PBP eboot_new.pbp EBOOT_BUILD.PBP

if [ ! -f Tutorial1.prx ]; then
  echo "Build failed: Tutorial1.prx was not produced."
  exit "${MAKEERR:-1}"
fi

# Pack to a temp name that PPSSPP is not holding open.
pack-pbp EBOOT_BUILD.PBP PARAM.SFO ICON0.png NULL NULL PIC1.png NULL Tutorial1.prx NULL

wait_writable_copy() {
  local src="$1"
  local dest="$2"
  local label="$3"
  local waited=0
  local max_wait=600   # seconds
  local interval=2

  if [ ! -f "$src" ]; then
    echo "Cannot copy $label — source missing: $src"
    return 1
  fi

  while true; do
    if cp -f "$src" "$dest" 2>/dev/null; then
      if [ "$waited" -gt 0 ]; then
        echo "Copied $label after ${waited}s wait."
      fi
      return 0
    fi
    if [ "$waited" -eq 0 ]; then
      echo "$label is locked (PPSSPP?). Waiting up to ${max_wait}s — close the emulator when you can..."
    elif [ $((waited % 30)) -eq 0 ]; then
      echo "  still waiting for $label... (${waited}s)"
    fi
    sleep "$interval"
    waited=$((waited + interval))
    if [ "$waited" -ge "$max_wait" ]; then
      echo "Timed out waiting for $label. Left build at $src"
      return 1
    fi
  done
}

DEST_DIR="Minecraft - Compiled"
mkdir -p "$DEST_DIR"

wait_writable_copy EBOOT_BUILD.PBP EBOOT.PBP "EBOOT.PBP" || true
wait_writable_copy EBOOT_BUILD.PBP "$DEST_DIR/EBOOT.PBP" "Minecraft - Compiled/EBOOT.PBP" || true

# Keep BUILD as fallback if root copy never unlocked; otherwise remove temp.
if [ -f EBOOT.PBP ] && cmp -s EBOOT_BUILD.PBP EBOOT.PBP 2>/dev/null; then
  rm -f EBOOT_BUILD.PBP
fi

# Regenerate the PPSSPP symbol map from the ELF this build just linked.
#
# Here rather than anywhere optional, because a stale map is worse than no map: it does not fail
# loudly, it silently names the wrong function, and a crash report built on it sends you to code that
# was never running. That is what happened -- every entry in the map the card carried was a
# `z_un_<address>` stub saved out of PPSSPP's own scan, against an older ELF, so `PC: 08810418` came
# back as a placeholder instead of `TextureManager::LoadPng`. Regenerating on every build means the
# map and the EBOOT it sits beside cannot drift apart. Non-fatal on purpose: a build that produced a
# playable EBOOT should not be failed by the symbol map alone.
if [ -f Tutorial1.elf ]; then
  bash tools/make-ppmap.sh || echo "make-ppmap: FAILED -- EBOOT.ppmap may not match this build."
fi

echo
echo "Playable EBOOT: EBOOT.PBP"
if [ -f "$DEST_DIR/EBOOT.PBP" ]; then
  echo "Also: $DEST_DIR/EBOOT.PBP"
fi
if [ -f EBOOT_BUILD.PBP ]; then
  echo "Temp build still at: EBOOT_BUILD.PBP (root was locked)"
fi
