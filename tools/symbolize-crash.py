#!/usr/bin/env python3
"""
Turn a PPSSPP crash screen into function names.

Why this exists
---------------
A PPSSPP "Game crashed / Invalid Memory Access" dialog only shows raw addresses:

    Access: Write Word at 3f00000c (sz: 4)
    PC: 08aad734
    Cur thread: user_main (attr 800040ff)
    z_un_08aad058 (08aad06c+6c8, pc: 08aad734 sp: 0bfbf998)
    z_un_089ee988 (089ee988+20, pc: 089ee9a8 sp: 0bfbf9c8)
    ...

which is useless on its own. This script maps those addresses onto the symbols in
the Tutorial1.elf that the build produces next to the EBOOT, so the same screen
reads as:

    PC   0x08aad734 -> _malloc_r +0x6cc

Usage
-----
    python tools/symbolize-crash.py 08aad734 08aad058 088849e8
    python tools/symbolize-crash.py --crash-text crash.txt
    python tools/symbolize-crash.py --crash-text crash.txt --elf path/to/Tutorial1.elf

You can also just paste the whole crash dialog text into a file; every 8-digit hex
address in it is picked up.

How it works
------------
The PSP loads a user module at 0x08804000, and the ELF is linked with its first
segment at 0x00000000, so `elf_address = runtime_address - 0x08804000`. The symbol
table is parsed straight out of the ELF32 file, so no PSP toolchain is required.
If WSL with binutils (or pspdev) is available the mangled C++ names are demangled;
otherwise the mangled name is still shown, which is enough to find the source file.
"""

import argparse
import os
import re
import struct
import subprocess
import sys

PSP_USER_MODULE_BASE = 0x08804000

# ---------------------------------------------------------------------------
# ELF32 parsing
# ---------------------------------------------------------------------------

SHT_SYMTAB = 2
STT_FUNC = 2
STT_OBJECT = 1


def read_cstring(data, offset):
    end = data.find(b"\0", offset)
    if end < 0:
        end = len(data)
    return data[offset:end].decode("utf-8", "replace")


def load_symbols(path):
    """Return a sorted list of (address, size, type, name) for defined symbols."""
    with open(path, "rb") as f:
        data = f.read()

    if data[:4] != b"\x7fELF":
        raise SystemExit(f"{path}: not an ELF file")
    if data[4] != 1:
        raise SystemExit(f"{path}: not a 32-bit ELF")

    (e_shoff,) = struct.unpack_from("<I", data, 0x20)
    (e_shentsize,) = struct.unpack_from("<H", data, 0x2E)
    (e_shnum,) = struct.unpack_from("<H", data, 0x30)
    (e_shstrndx,) = struct.unpack_from("<H", data, 0x32)

    sections = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        (name_off, stype, flags, addr, offset, size, link, info, align, entsize) = struct.unpack_from(
            "<IIIIIIIIII", data, off
        )
        sections.append(
            {
                "name_off": name_off,
                "type": stype,
                "addr": addr,
                "offset": offset,
                "size": size,
                "link": link,
                "entsize": entsize,
            }
        )

    shstr = sections[e_shstrndx]
    for s in sections:
        s["name"] = read_cstring(data, shstr["offset"] + s["name_off"])

    syms = []
    for s in sections:
        if s["type"] != SHT_SYMTAB:
            continue
        strtab = sections[s["link"]]
        entsize = s["entsize"] or 16
        count = s["size"] // entsize
        for i in range(count):
            off = s["offset"] + i * entsize
            st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from(
                "<IIIBBH", data, off
            )
            if st_shndx == 0:  # undefined
                continue
            if st_name == 0:
                continue
            sym_type = st_info & 0xF
            syms.append((st_value, st_size, sym_type, read_cstring(data, strtab["offset"] + st_name)))

    syms.sort(key=lambda t: t[0])
    return syms


def demangle(names):
    """Best-effort C++ demangling. Falls back to the mangled names."""
    unique = sorted(set(n for n in names if n.startswith("_Z")))
    if not unique:
        return {}
    for cmd in (
        ["wsl", "-e", "bash", "-c", "export PATH=$HOME/pspdev/bin:$PATH; psp-c++filt"],
        ["wsl", "-e", "c++filt"],
    ):
        try:
            proc = subprocess.run(
                cmd,
                input="\n".join(unique),
                capture_output=True,
                text=True,
                timeout=60,
            )
            out = proc.stdout.strip().split("\n")
            if len(out) == len(unique):
                return dict(zip(unique, out))
        except Exception:
            continue
    return {}


def find_symbol(syms, addr):
    """Binary search for the symbol containing addr."""
    lo, hi, best = 0, len(syms) - 1, None
    while lo <= hi:
        mid = (lo + hi) // 2
        if syms[mid][0] <= addr:
            best = mid
            lo = mid + 1
        else:
            hi = mid - 1
    if best is None:
        return None
    return syms[best]


# ---------------------------------------------------------------------------

ADDR_RE = re.compile(r"\b0?8[0-9a-fA-F]{6}\b")

# Some large functions (the ones the linker did not give a size for, e.g. the
# generator code) sit in .text without their own symbol, so the nearest preceding
# symbol can be tens of kilobytes away. An offset that large means "somewhere in an
# unnamed function", not "deep inside buildmobVerts", so say so rather than lie.
UNRELIABLE_OFFSET = 0x8000


def ocr_image(path):
    """OCR a screenshot with the Windows OCR engine (tools/ocr-image.ps1)."""
    helper = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ocr-image.ps1")
    if not os.path.exists(helper):
        return ""
    try:
        proc = subprocess.run(
            ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", helper, path],
            capture_output=True, text=True, timeout=180,
        )
        if proc.returncode != 0:
            sys.stderr.write(proc.stderr)
        return proc.stdout
    except Exception as e:
        sys.stderr.write(f"OCR failed: {e}\n")
        return ""


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    default_elf = os.path.normpath(os.path.join(here, "..", "Tutorial1.elf"))

    ap = argparse.ArgumentParser(description="Map PPSSPP crash addresses onto Tutorial1.elf symbols.")
    ap.add_argument("addresses", nargs="*", help="runtime addresses, e.g. 08aad734")
    ap.add_argument("--crash-text", help="file containing the pasted PPSSPP crash screen")
    ap.add_argument("--crash-image", help="screenshot of the PPSSPP crash screen (OCR'd via Windows OCR)")
    ap.add_argument("--elf", default=default_elf, help=f"path to Tutorial1.elf (default: {default_elf})")
    ap.add_argument("--base", default=hex(PSP_USER_MODULE_BASE),
                    help="runtime load address of the module (default 0x08804000)")
    args = ap.parse_args()

    if not os.path.exists(args.elf):
        raise SystemExit(
            f"{args.elf} not found.\n"
            "Build the game first (the ELF is produced next to the EBOOT), or pass --elf."
        )

    base = int(args.base, 0)

    addrs = list(args.addresses)
    if args.crash_text:
        text = open(args.crash_text, "r", errors="replace").read()
        addrs += ADDR_RE.findall(text)
    if args.crash_image:
        text = ocr_image(args.crash_image)
        found = ADDR_RE.findall(text)
        print(f"OCR found {len(found)} address(es) in {os.path.basename(args.crash_image)}")
        addrs += found

    if not addrs:
        raise SystemExit("No addresses given. Pass some, or --crash-text/--crash-image <file>.")

    syms = load_symbols(args.elf)
    funcs = [s for s in syms if s[2] == STT_FUNC]
    print(f"{args.elf}: {len(syms)} symbols ({len(funcs)} functions), base 0x{base:08x}\n")

    resolved = []
    for a in addrs:
        try:
            rt = int(a, 16)
        except ValueError:
            continue
        v = rt - base
        if v < 0 or v > 0x8000000:
            print(f"  0x{rt:08x} -> outside the module (not a code address?)")
            continue
        hit = find_symbol(funcs, v)
        if hit is None:
            print(f"  0x{rt:08x} -> (below every symbol)")
            continue
        sym_addr, sym_size, _t, name = hit
        resolved.append((rt, v, sym_addr, sym_size, name))

    dm = demangle([r[4] for r in resolved])
    for rt, v, sym_addr, sym_size, name in resolved:
        pretty = dm.get(name, name)
        off = rt - base - sym_addr
        note = ""
        if (sym_size and off >= sym_size) or off > UNRELIABLE_OFFSET:
            note = "   <- offset too large for this symbol: really an unnamed function in the same .text blob"
        print(f"  0x{rt:08x} -> {pretty} +0x{off:x}{note}")

    # A hint at the shape of the failure, since the raw screen is cryptic.
    if resolved:
        top = dm.get(resolved[0][4], resolved[0][4])
        if "malloc" in top or "free" in top:
            print(
                "\nNote: the innermost frame is the allocator. That normally means a heap block\n"
                "was overrun (or freed twice) earlier in the run and malloc only tripped over the\n"
                "damage now. Rebuild with MC_DEBUG_MODE 1 (Scripts/DebugConfig.h) and check Log.txt\n"
                "for the last 'heap <checkpoint>' line before the crash -- the guard bands report the\n"
                "block size and sequence number of the buffer that was overrun."
            )


if __name__ == "__main__":
    main()
