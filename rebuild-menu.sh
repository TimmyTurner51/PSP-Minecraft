#!/bin/bash
set -euo pipefail
export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"
cd "$(dirname "$0")"
make -f MakefileGame \
  Scripts/StateMenu.o \
  Scripts/StatePlay.o \
  Scripts/StatePlayCreative.o \
  Scripts/CraftWorld2.o \
  source/Aurora/Graphics/intraFont/intraFont.o
make -f MakefileGame
cp -f EBOOT.PBP "Minecraft - Compiled/EBOOT.PBP"
echo "MENU REBUILD OK"
