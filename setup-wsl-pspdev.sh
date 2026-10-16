#!/bin/bash
set -euo pipefail

MARKER='# PSPDEV toolchain (LameCraft / MC PSP Project)'
EXPORTS=$'export PSPDEV="$HOME/pspdev"\nexport PATH="$PATH:$PSPDEV/bin"'

if ! grep -Fq "$MARKER" "$HOME/.bashrc" 2>/dev/null; then
  printf '\n%s\n%s\n' "$MARKER" "$EXPORTS" >> "$HOME/.bashrc"
  echo "Added PSPDEV to ~/.bashrc"
else
  echo "PSPDEV already in ~/.bashrc"
fi

export PSPDEV="$HOME/pspdev"
export PATH="$PSPDEV/bin:$PATH"

if [ ! -x "$PSPDEV/bin/psp-gcc" ]; then
  echo "ERROR: $PSPDEV/bin/psp-gcc is missing"
  exit 1
fi

echo "psp-config --pspdev-path = $(psp-config --pspdev-path)"
echo "psp-config --pspsdk-path = $(psp-config --pspsdk-path)"
psp-gcc --version | head -n 1
echo "OK"
