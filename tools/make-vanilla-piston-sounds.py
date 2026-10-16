#!/usr/bin/env python3
"""
Pull the piston extend/retract WAVs out of the vanilla Minecraft 1.7.3 assets.

Java 1.7.3's BlockPistonBase plays, at the piston's centre:

    extending   "tile.piston.out"  volume 0.5, pitch rand*0.25 + 0.6
    retracting  "tile.piston.in"   volume 0.5, pitch rand*0.15 + 0.6

and the two samples behind those names are sounds/tile/piston/out.ogg and in.ogg in the
official assets index. Both are written into both quality tiers, the same way the button
and bowhit samples are:

    Assets/Sounds/other/piston_out.wav      <-  sounds/tile/piston/out.ogg
    Assets/Sounds/other/piston_in.wav       <-  sounds/tile/piston/in.ogg
    Assets/SoundsLow/other/...              (22 kHz mono, the 32 MB build's tier)

Needs the official assets index plus ffmpeg:

    python tools/make-vanilla-piston-sounds.py [--minecraft DIR] [--ffmpeg PATH]

Run it from anywhere; it writes into the source tree next to this script.
"""

import argparse
import json
import os
import subprocess

# vanilla asset -> the name the game loads
SOURCES = {
    "sounds/tile/piston/out.ogg": "piston_out",
    "sounds/tile/piston/in.ogg": "piston_in",
}

# asset tier on disk -> ffmpeg output format
TIERS = {
    "Sounds": ["-ac", "2", "-ar", "44100"],
    "SoundsLow": ["-ac", "1", "-ar", "22050"],
}


def main():
    game_root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--minecraft", default=os.path.join(os.environ.get("APPDATA", ""), ".minecraft"),
                    help="Minecraft data directory (default: %%APPDATA%%/.minecraft)")
    ap.add_argument("--index", default="1.7.3.json",
                    help="asset index to read the sounds from (default: 1.7.3.json)")
    ap.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable (default: ffmpeg on PATH)")
    args = ap.parse_args()

    index_path = os.path.join(args.minecraft, "assets", "indexes", args.index)
    objects = os.path.join(args.minecraft, "assets", "objects")
    if not os.path.exists(index_path):
        raise SystemExit(f"{index_path} not found. Pass --minecraft pointing at your .minecraft.")

    index = json.load(open(index_path, "r", encoding="utf-8"))["objects"]

    for key, base in SOURCES.items():
        entry = index.get(key)
        if entry is None:
            raise SystemExit(f"{key} is not in {args.index}; choose another --index.")
        src = os.path.join(objects, entry["hash"][:2], entry["hash"])
        if not os.path.exists(src):
            raise SystemExit(f"{src} missing (asset objects incomplete).")

        for tier, fmt in TIERS.items():
            # Both tiers live under Assets/: Assets/Sounds/other (full) and
            # Assets/SoundsLow/other (22 kHz mono for the 32 MB build).
            out_dir = os.path.join(game_root, "Assets", tier, "other")
            os.makedirs(out_dir, exist_ok=True)
            out = os.path.join(out_dir, base + ".wav")
            cmd = [args.ffmpeg, "-y", "-loglevel", "error", "-i", src, *fmt,
                   "-sample_fmt", "s16", "-acodec", "pcm_s16le", out]
            proc = subprocess.run(cmd, capture_output=True, text=True)
            if proc.returncode != 0 or not os.path.exists(out):
                raise SystemExit(f"ffmpeg failed for {out}: {proc.stderr.strip()}")
            print(f"wrote {out} ({os.path.getsize(out)} bytes)")

    print("\nNothing to copy into the distribution folder: Minecraft - Compiled/Assets is the same")
    print("tree as this one (they are linked), so the new WAVs are already on both sides.")
    print("Run tools/check-sound-loads.py to confirm the samples landed in both tiers.")


if __name__ == "__main__":
    main()
