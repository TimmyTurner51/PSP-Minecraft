#!/bin/bash
# Regenerate EBOOT.ppmap from the ELF the current build just produced.
#
#   tools/make-ppmap.sh
#
# Why this exists: EBOOT.ppmap is a PPSSPP symbol map, and nothing in the build ever wrote one --
# it was a file someone saved out of PPSSPP's debugger once and then copied around by hand. So it
# described whichever build was current on the day it was saved, while the EBOOT it sat beside moved
# on, and a crash reported against the new build would have been symbolised with the old addresses.
# Underneath the staleness it was the *weaker* of the two maps: PPSSPP names every function it finds
# `z_un_<address>` when it has no name for it, and a map saved from PPSSPP's own scan contains
# nothing but those -- which is why the console crash read `z_un_088102a4`. The ELF carries the real
# mangled C++ names, so generating from it gives both the right addresses and names worth reading.
#
# Format (PPSSPP's Core/Debugger/SymbolMap.cpp, SaveSymbolMap/LoadSymbolMap):
#
#   .text
#   .module <index hex> <base %08x> <size %08x> <name>
#   <offset %08x> <size %08x> <module index hex> <type> <name>
#
# Offsets are relative to the module base, which is where PPSSPP loads the game PRX. The loader
# reconstructs the absolute address as base + offset, so the base has to be the real load address;
# 0x08804000 is what Tutorial1.map and the existing map both use (the module's `_main` is at vaddr
# 0x20, and PPSSPP reports it at 0x08804020).
#
# Everything is gzipped, which is what PPSSPP's loader expects (it opens the file with gzopen and
# also accepts plain zlib). -n keeps the gzip header free of a timestamp so the file is reproducible.
#
# No hex arithmetic is done in awk on purpose: Ubuntu's awk is mawk, which has no strtonum(), so the
# hex values are only ever compared as strings there and added in bash, which does understand 16#.
set -euo pipefail

export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

cd "$(dirname "$0")/.."

ELF=Tutorial1.elf
OUT=EBOOT.ppmap
BASE=0x08804000
MODULE_NAME='"Tutorial1"'

if [ ! -f "$ELF" ]; then
    echo "make-ppmap: $ELF not found -- build first (a clean build removes it until the link runs)."
    exit 1
fi

TMPTXT="$(mktemp)"
SYMFILE="$(mktemp)"
trap 'rm -f "$TMPTXT" "$SYMFILE"' EXIT

# Module size: the whole loaded image, not just the part the functions reach. Taken from the ELF's
# loadable segments (vaddr + memsz, which for a PRX are already relative to the module base) and
# rounded up to 4 KB. This is what bounds address -> module lookups, so it has to cover everything.
# Cross-check: the highest symbol end (addr+size) over all defined symbols lands at 0x3d0c9c, the
# same byte as the second segment's end, i.e. the linker leaves nothing after the last .bss object.
MODULE_SIZE=0
while read -r vaddr memsz; do
    [ -z "$vaddr" ] && continue
    # readelf -lW prints these 0x-prefixed; bash's 16# wants the digits alone.
    end=$(($((16#${vaddr#0x})) + $((16#${memsz#0x}))))
    [ "$end" -gt "$MODULE_SIZE" ] && MODULE_SIZE=$end
done < <(psp-readelf -lW "$ELF" 2>/dev/null | awk '$1 == "LOAD" { print $3, $6 }')

if [ "$MODULE_SIZE" -le 0 ]; then
    echo "make-ppmap: could not read the loaded image size out of $ELF."
    exit 1
fi
MODULE_SIZE=$(( (MODULE_SIZE + 0xfff) & ~0xfff ))
MODULE_SIZE_HEX=$(printf '%08x' "$MODULE_SIZE")

printf '.text\n' > "$TMPTXT"
printf '.module 1 %08x %s %s\n' "$BASE" "$MODULE_SIZE_HEX" "$MODULE_NAME" >> "$TMPTXT"

# Function symbols only, and only the ones with a non-zero size: T/t are .text functions, W/w the
# weak ones. Data symbols are deliberately left out -- the map this replaces carried none, so nothing
# downstream has ever expected them, and a data address mistaken for a function disassembles as
# garbage. `psp-nm -S` prints the size as eight hex digits, so "not zero" is a string test.
psp-nm -S --defined-only "$ELF" 2>/dev/null | awk '
    ($3 == "T" || $3 == "t" || $3 == "W" || $3 == "w") && $2 !~ /^0+$/ && NF == 4 {
        printf "%s %s 1 1 %s\n", $1, $2, $4;
    }' > "$SYMFILE"

SYMCOUNT="$(wc -l < "$SYMFILE")"
if [ "$SYMCOUNT" -le 0 ]; then
    echo "make-ppmap: $ELF has no sized function symbols -- refusing to write an empty map."
    exit 1
fi

# Sorted, because PPSSPP's own maps are and a diff against one is then readable.
sort "$SYMFILE" >> "$TMPTXT"

gzip -9 -n -c "$TMPTXT" > "$OUT"

echo "make-ppmap: $OUT from $ELF -- $SYMCOUNT function(s), module base $(printf '0x%08x' "$BASE") size 0x$MODULE_SIZE_HEX"
ls -la "$OUT"

# Keep the copy the deploy scripts and the release packaging read.
DEST_DIR="Minecraft - Compiled"
if [ -d "$DEST_DIR" ]; then
    cp -f "$OUT" "$DEST_DIR/$OUT"
    echo "make-ppmap: also written to $DEST_DIR/$OUT"
fi
