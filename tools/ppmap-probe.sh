#!/bin/bash
# scratch probe: what does the ELF give us for a symbol map, and what is the module's extent?
set -uo pipefail
export PSPDEV="${PSPDEV:-$HOME/pspdev}"
export PATH="$PSPDEV/bin:$PATH"

D="/mnt/c/Users/minec/Documents/MC PSP Project/Source Code V3.0.0/PSP Minecraft Source Code"
cd "$D"

echo "=== program headers (loadable extent) ==="
psp-readelf -lW Tutorial1.elf 2>/dev/null | awk '$1 == "LOAD" { printf "LOAD vaddr=0x%s memsz=0x%s flags=%s\n", $3, $6, $7 }'

echo
echo "=== highest symbol end (addr+size) across all defined symbols ==="
psp-nm -S --defined-only Tutorial1.elf 2>/dev/null | awk 'NF>=4 && $1 ~ /^[0-9a-f]+$/ {a=strtonum("0x"$1); s=strtonum("0x"$2); if(a+s>m){m=a+s; n=$4; ta=$1; ts=$2}} END{printf "max end = 0x%x  (last %s at %s size %s)\n", m, n, ta, ts}'

echo
echo "=== size of .text / .data / .bss / .rodata ==="
psp-readelf -SW Tutorial1.elf 2>/dev/null | grep -E "\.text|\.data|\.bss|\.rodata" | head -8

echo
echo "=== function-symbol counts by type (all / sized) ==="
for t in T t W w; do
  all=$(psp-nm -S --defined-only Tutorial1.elf 2>/dev/null | awk -v T="$t" '$3==T' | wc -l)
  sized=$(psp-nm -S --defined-only Tutorial1.elf 2>/dev/null | awk -v T="$t" '$3==T && $2 !~ /^0+$/' | wc -l)
  printf "%s : %4s total, %4s sized\n" "$t" "$all" "$sized"
done

echo
echo "=== records that do NOT have exactly 4 fields (would break the format) ==="
psp-nm -S --defined-only Tutorial1.elf 2>/dev/null | awk '($3=="T"||$3=="t"||$3=="W"||$3=="w") && $2 !~ /^0+$/ && NF != 4 { print NF": "$0 }' | head -10

echo
echo "=== sample of sized .text functions (first 8, and 5 more with real names) ==="
psp-nm -S --defined-only Tutorial1.elf 2>/dev/null | awk '$3=="T" && $2 !~ /^0+$/' | head -8
echo "  ..."
psp-nm -S --defined-only Tutorial1.elf 2>/dev/null | awk '($3=="T"||$3=="t") && $2 !~ /^0+$/ && $4 ~ /CraftWorld|LightFlood|RebuildFullMeshChunk/' | head -8

echo
echo "=== names that would collide at the same address ==="
psp-nm -S --defined-only Tutorial1.elf 2>/dev/null | awk '($3=="T"||$3=="t") && $2 !~ /^0+$/ { print $1 }' | sort | uniq -d | wc -l

echo
echo "=== _main ==="
psp-nm -S --defined-only Tutorial1.elf 2>/dev/null | grep -w "_main"
