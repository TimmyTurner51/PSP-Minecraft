#!/bin/bash
set -euo pipefail
export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"
cd "$(dirname "$0")"
pack-pbp EBOOT_BUILD.PBP PARAM.SFO ICON0.png NULL NULL PIC1.png NULL Tutorial1.prx NULL
ls -la EBOOT_BUILD.PBP
