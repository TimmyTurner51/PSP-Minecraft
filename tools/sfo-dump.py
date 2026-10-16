#!/usr/bin/env python3
"""
Dump a PARAM.SFO and, with --diff, compare two of them.

Why this exists
---------------
A homebrew EBOOT that the console refuses to launch gives almost nothing to work with: no log, no
screen, no error. One of the few things that can actually be compared in that situation is the
PARAM.SFO, because an EBOOT that will not load is usually refused before any of its own code runs,
which leaves the packaging as the only suspect.

The field that has bitten this project is MEMSIZE. PSPSDK does not set it from the makefile directly;
it derives it from PSP_FW_VERSION and PSP_LARGE_MEMORY (see the EXPAND_MEMORY block in the SDK's
build.mak) and passes it to mksfo. A build that asks for expanded memory on a console that cannot
provide it is not a subtle failure -- it is a module that never starts.

Usage
-----
    # dump one
    tools/sfo-dump.py PARAM.SFO

    # unpack an EBOOT and dump it in one step
    tools/sfo-dump.py --pbp EBOOT.PBP

    # compare a suspect build against a build known to work on the same console
    tools/sfo-dump.py --diff working/PARAM.SFO suspect/PARAM.SFO

Needs the PSP toolchain only for --pbp (it shells out to unpack-pbp).
"""

import argparse
import os
import struct
import subprocess
import sys

PSF_MAGIC = 0x46535000

# data_fmt values, from the PSF format as Sony defined it.
FMT_BINARY = 0x0004
FMT_STRING = 0x0204
FMT_INT32 = 0x0404

# Fields whose plain integer means nothing to a reader. MEMSIZE is the one that decides whether a
# PSP-1000 will take the module at all, so it is spelled out rather than left as a number.
MEMSIZE_MEANING = {
    0: "0  -- normal partition (32 MB user memory)",
    1: "1  -- EXPANDED memory requested (needs a PSP-2000/3000, 64 MB)",
    2: "2  -- extra memory as available",
}


def read_sfo(path):
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < 20:
        raise SystemExit(f"{path}: too short to be a PARAM.SFO")

    magic, version, key_start, data_start, count = struct.unpack_from("<IIIII", data, 0)

    if magic != PSF_MAGIC:
        raise SystemExit(f"{path}: bad magic {magic:#010x}, expected {PSF_MAGIC:#010x}")

    fields = []
    for i in range(count):
        off = 20 + i * 16
        key_off, fmt, data_len, data_max, data_off = struct.unpack_from("<HHIII", data, off)

        # The key is a NUL-terminated string in the key table.
        key_at = key_start + key_off
        key_end = data.find(b"\0", key_at)
        key = data[key_at:key_end].decode("utf-8", "replace")

        raw = data[data_start + data_off : data_start + data_off + data_len]

        if fmt == FMT_STRING:
            value = raw.split(b"\0")[0].decode("utf-8", "replace")
        elif fmt == FMT_INT32:
            value = struct.unpack_from("<i", raw + b"\0" * 4)[0]
        else:
            value = raw.hex()

        fields.append((key, fmt, value))

    # Sorted, because the SFO's own order is whatever mksfo was given and comparing two dumps by eye
    # is the entire point of this tool.
    fields.sort(key=lambda f: f[0])
    return version, fields


def unpack_pbp(pbp_path, workdir):
    """Unpack a PBP and return the PARAM.SFO inside it."""
    os.makedirs(workdir, exist_ok=True)

    # unpack-pbp writes into the current directory, so run it there.
    pspdev = os.environ.get("PSPDEV") or os.path.expanduser("~/pspdev")
    tool = os.path.join(pspdev, "bin", "unpack-pbp")
    if not os.path.exists(tool):
        tool = "unpack-pbp"  # hope it is on PATH

    proc = subprocess.run([tool, os.path.abspath(pbp_path)], cwd=workdir,
                          capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit(f"unpack-pbp failed:\n{proc.stdout}\n{proc.stderr}")

    sfo = os.path.join(workdir, "PARAM.SFO")
    if not os.path.exists(sfo):
        raise SystemExit(f"unpack-pbp produced no PARAM.SFO in {workdir}\n{proc.stdout}")

    return sfo


def print_fields(path, fields, version):
    print(f"{path}  (version {version:#010x})")
    for key, fmt, value in fields:
        if key == "MEMSIZE" and isinstance(value, int):
            print(f"  {key:<16} {MEMSIZE_MEANING.get(value, value)}")
        elif isinstance(value, int):
            print(f"  {key:<16} {value}")
        else:
            print(f"  {key:<16} {value}")


def diff(a_path, a_fields, b_path, b_fields):
    a = {k: v for k, _f, v in a_fields}
    b = {k: v for k, _f, v in b_fields}

    print(f"A = {a_path}")
    print(f"B = {b_path}")
    print()

    only_a = sorted(set(a) - set(b))
    only_b = sorted(set(b) - set(a))
    common = sorted(set(a) & set(b))

    if not only_a and not only_b:
        print("(same set of fields)")
    for k in only_a:
        print(f"  only in A: {k} = {a[k]}")
    for k in only_b:
        print(f"  only in B: {k} = {b[k]}")

    print()
    print("differences:")
    found = False
    for k in common:
        if a[k] != b[k]:
            found = True
            print(f"  {k:<16} A={a[k]}   B={b[k]}")
            if k == "MEMSIZE":
                print(f"  {'':<16} A -> {MEMSIZE_MEANING.get(a[k], a[k])}")
                print(f"  {'':<16} B -> {MEMSIZE_MEANING.get(b[k], b[k])}")
    if not found:
        print("  (none)")

    # The whole reason this tool exists, stated when it is the thing that differs.
    if "MEMSIZE" in common and a["MEMSIZE"] != b["MEMSIZE"]:
        print()
        print("NOTE: MEMSIZE differs. PSPSDK derives it from PSP_FW_VERSION and PSP_LARGE_MEMORY:")
        print("        PSP_FW_VERSION > 390        -> MEMSIZE 2")
        print("        PSP_FW_VERSION <= 390:")
        print("          PSP_LARGE_MEMORY=1        -> MEMSIZE 1  (expanded memory requested)")
        print("          PSP_LARGE_MEMORY=2        -> MEMSIZE 2")
        print("          (unset)                   -> MEMSIZE 0")
        print("      A module that asks for expanded memory will not start on a console that cannot")
        print("      provide it, and gives no error beyond refusing to load.")


def main():
    ap = argparse.ArgumentParser(description="Dump or compare the PARAM.SFO inside a PSP EBOOT.")
    ap.add_argument("files", nargs="+", help="PARAM.SFO paths (or EBOOT.PBP paths with --pbp)")
    ap.add_argument("--pbp", action="store_true", help="treat the arguments as EBOOT.PBP files to unpack first")
    ap.add_argument("--diff", action="store_true", help="compare exactly two files")
    ap.add_argument("--workdir", default=None, help="scratch directory for --pbp (default: beside the file)")
    args = ap.parse_args()

    paths = []
    for i, p in enumerate(args.files):
        if args.pbp:
            work = args.workdir or os.path.join(os.path.dirname(os.path.abspath(p)), f"_pbp_unpack_{i}")
            print(f"unpacking {p} -> {work}")
            paths.append(unpack_pbp(p, work))
        else:
            paths.append(p)

    if args.diff:
        if len(paths) != 2:
            raise SystemExit("--diff needs exactly two files")
        va, fa = read_sfo(paths[0])
        vb, fb = read_sfo(paths[1])
        print()
        diff(paths[0], fa, paths[1], fb)
        return

    for p in paths:
        version, fields = read_sfo(p)
        print_fields(p, fields, version)
        print()


if __name__ == "__main__":
    main()
