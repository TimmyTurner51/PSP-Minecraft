#!/bin/bash
set -euo pipefail
export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"
cd "$(dirname "$0")"
make -f MakefileGame source/Aurora/Graphics/intraFont/intraFont.o source/Aurora/Graphics/RenderManager.o
make -f MakefileGame
cp -f EBOOT.PBP "Minecraft - Compiled/EBOOT.PBP"
echo "FONT REBUILD OK"
