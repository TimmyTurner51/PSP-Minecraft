#!/usr/bin/env python3
"""
Regenerate the arrow-impact WAVs from the vanilla Minecraft 1.7.3 assets.

An arrow that landed made no sound of any kind: the only bow audio the game had was
random/bow -- the release whoosh, already present as Assets/Sounds/other/bow.wav and
played by SoundManager::PlayBowSound -- so a shot flew, thudded into a wall and vanished
in total silence. Java 1.7.3 gives the impact two different samples depending on what was
hit, and both are imported here:

    Assets/Sounds/other/bowhit1..4.wav     <-  sounds/random/bowhit{1,2,3,4}.ogg
    Assets/Sounds/other/bowhit_flesh.wav   <-  sounds/random/successful_hit.ogg

The four bowhit clips are the block impact (a shot sticking in dirt, stone or wood) and
are picked at random the way vanilla picks them; successful_hit is the wetter thwack the
arrow makes on a body, used for both a mob the player shot and the player taking a
skeleton's arrow. Splitting them is why the rename is not just "bowhit": the two paths
sound different on purpose and the code picks between them by what the arrow hit.

Both quality tiers are written (Sounds = 44.1 kHz stereo, SoundsLow = 22 kHz mono, which
the 32 MB build picks up through SoundManager's LoadSfx), exactly as
make-vanilla-button-sounds.py does for the redstone buttons.

Needs the official assets index plus ffmpeg:

    python tools/make-vanilla-bowhit-sounds.py [--minecraft DIR] [--ffmpeg PATH]

Run it from anywhere; it writes into the source tree next to this script.
"""

import argparse
import json
import os
import subprocess

# vanilla asset -> the name the game loads (assets/other/<name>.wav)
SOURCES = {
    "sounds/random/bowhit1.ogg": "bowhit1",
    "sounds/random/bowhit2.ogg": "bowhit2",
    "sounds/random/bowhit3.ogg": "bowhit3",
    "sounds/random/bowhit4.ogg": "bowhit4",
    "sounds/random/successful_hit.ogg": "bowhit_flesh",
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

    print("\nRun tools/check-wav-levels.py before shipping: the mixer sums every playing voice,")
    print("and a volley of arrows can have several of these playing at once.")
    print("\nNothing to copy into the distribution folder: Minecraft - Compiled/Assets is the same")
    print("tree as this one (they are linked), so the new WAVs are already on both sides.")


if __name__ == "__main__":
    main()
