#!/bin/bash
set -euo pipefail
export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"
cd "$(dirname "$0")"
make -f MakefileGame Scripts/StatePlay.o Scripts/StatePlayCreative.o
make -f MakefileGame
cp -f EBOOT.PBP "Minecraft - Compiled/EBOOT.PBP"
echo "PLAY REBUILD OK"
