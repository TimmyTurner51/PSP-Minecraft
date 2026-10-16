#!/bin/bash
set -euo pipefail

export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

cd "$(dirname "$0")"

make
