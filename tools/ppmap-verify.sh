#!/bin/bash
# scratch verifier: is the generated EBOOT.ppmap well-formed, and does its address mapping agree
# with the addresses PPSSPP actually printed for the console crash?
set -uo pipefail

D="/mnt/c/Users/minec/Documents/MC PSP Project/Source Code V3.0.0/PSP Minecraft Source Code"
cd "$D"

echo "=== gzip integrity ==="
gzip -t EBOOT.ppmap && echo "gzip -t OK"
file EBOOT.ppmap

echo
echo "=== header ==="
zcat EBOOT.ppmap | head -n 4

echo
echo "=== line count / record count ==="
total=$(zcat EBOOT.ppmap | wc -l)
echo "lines: $total"

echo
echo "=== every record parses as '%08x %08x <int> <int> <name>' ==="
zcat EBOOT.ppmap | awk '
    NR <= 2 { next }
    {
        if (NF != 5) { bad++; print "BAD FIELD COUNT(" NF "): " $0; next }
        if ($1 !~ /^[0-9a-f]{8}$/) { bad++; print "BAD ADDR: " $0 }
        if ($2 !~ /^[0-9a-f]{8}$/) { bad++; print "BAD SIZE: " $0 }
        if ($3 !~ /^[0-9]+$/ || $4 !~ /^[0-9]+$/) { bad++; print "BAD INDEX/TYPE: " $0 }
        if ($4 != 1) { bad++; print "BAD TYPE(VALUE): " $0 }
        n++
    }
    END { printf "records: %d, malformed: %d\n", n, bad+0 }'

echo
echo "=== names are unique-address safe: any name with whitespace or leading dash ==="
zcat EBOOT.ppmap | awk 'NR>2 && $5 ~ /^[^A-Za-z_]/ { print "odd: " $5 }' | head -5
echo "(none above = good)"

echo
echo "=== type distribution ==="
zcat EBOOT.ppmap | awk 'NR>2 { print $4 }' | sort | uniq -c

echo
echo "=== are the names real, not z_un_ ? ==="
echo "z_un_ records: $(zcat EBOOT.ppmap | awk 'NR>2 && $5 ~ /^z_un_/' | wc -l)"
zcat EBOOT.ppmap | awk 'NR>2' | shuf -n 6 --random-source=<(yes)

echo
echo "=== the crash frame: PC 0x088102a4 -> offset 0xC2A4, and the jump target 0x08810418 -> 0xC418 ==="
for off in 0000c2a4 0000c418; do
    echo "-- offset 0x$off --"
    zcat EBOOT.ppmap | awk -v t="$off" '
        NR > 2 {
            start = strtonum("0x" $1); size = strtonum("0x" $2);
            want = strtonum("0x" t);
            if (want >= start && want < start + size) printf "  contains: %s size=%s %s\n", $1, $2, $5;
        }' | head -n 4
done

echo
echo "=== _main and the light flood symbols ==="
zcat EBOOT.ppmap | awk 'NR>2 && ($5=="_main" || $5 ~ /LightFlood|LightTravel|SetLigtSourcePosition/)' | head -n 12

echo
echo "=== module record ==="
zcat EBOOT.ppmap | awk 'NR==2'
