#!/usr/bin/env python3
"""
Import a vanilla mob's sounds from the official 1.7.3 assets into the game's WAV tiers.

The engine loads mob audio as plain PCM WAVs under Assets/Sounds/mobs/<mob>/ and
Assets/SoundsLow/mobs/<mob>/; the vanilla client ships the same clips as Vorbis inside the
asset index. This bridges the two, exactly like tools/make-vanilla-button-sounds.py does
for the GUI clicks, and writes every tier/root it can find so the source tree and the
distributable copy stay in step.

Why it matters here: the built game had cow, creeper, pig, sheep and zombie audio but no
chicken audio at all, so chickens were silent. 1.7.3 does ship a full chicken set
(hurt1/hurt2, say1-3, step1-2 and the plop it makes when laying an egg); it just was never
converted.

    python tools/make-vanilla-mob-sounds.py chicken
    python tools/make-vanilla-mob-sounds.py --all
    python tools/make-vanilla-mob-sounds.py chicken --list

Needs the official assets index plus ffmpeg on PATH.
"""

import argparse
import json
import os
import subprocess
import sys

# vanilla asset name -> the basename the engine loads (assets/<mob>/<name>.wav)
MOBS = {
    "chicken": {
        "sounds/mob/chicken/hurt1.ogg": "hurt1",
        "sounds/mob/chicken/hurt2.ogg": "hurt2",
        "sounds/mob/chicken/say1.ogg": "say1",
        "sounds/mob/chicken/say2.ogg": "say2",
        "sounds/mob/chicken/say3.ogg": "say3",
        "sounds/mob/chicken/step1.ogg": "step1",
        "sounds/mob/chicken/step2.ogg": "step2",
        "sounds/mob/chicken/plop.ogg": "plop",
    },
    # Cow and zombie were converted into Assets/Sounds/ but only partly into Assets/SoundsLow/, so
    # the PSP-1000 tier went silent partway through a cow's moo. Both sets below are complete, so
    # re-running this mob fills the low tier in.
    "cow": {
        "sounds/mob/cow/hurt1.ogg": "hurt1",
        "sounds/mob/cow/hurt2.ogg": "hurt2",
        "sounds/mob/cow/say1.ogg": "say1",
        "sounds/mob/cow/say2.ogg": "say2",
        "sounds/mob/cow/say3.ogg": "say3",
        "sounds/mob/cow/say4.ogg": "say4",
    },
    "zombie": {
        "sounds/mob/zombie/hurt1.ogg": "hurt1",
        "sounds/mob/zombie/hurt2.ogg": "hurt2",
        "sounds/mob/zombie/say1.ogg": "say1",
        "sounds/mob/zombie/say2.ogg": "say2",
        "sounds/mob/zombie/say3.ogg": "say3",
        "sounds/mob/zombie/death.ogg": "death",
    },
    # The spider shipped with nine clips: four idles, four footfalls and a death rattle. It has no
    # hurt clip of its own -- 1.7.3 points getHurtSound() at mob.spider.say -- so the hurt path
    # plays one of the say clips, the same way the creeper's does.
    "spider": {
        "sounds/mob/spider/say1.ogg": "say1",
        "sounds/mob/spider/say2.ogg": "say2",
        "sounds/mob/spider/say3.ogg": "say3",
        "sounds/mob/spider/say4.ogg": "say4",
        "sounds/mob/spider/step1.ogg": "step1",
        "sounds/mob/spider/step2.ogg": "step2",
        "sounds/mob/spider/step3.ogg": "step3",
        "sounds/mob/spider/step4.ogg": "step4",
        "sounds/mob/spider/death.ogg": "death",
    },
    # The skeleton ships twelve clips: three idles, four hurts, four footfalls and a death rattle.
    # Unlike the spider it does have hurt clips of its own -- 1.7.3 getHurtSound() is
    # mob.skeleton.hurt -- so the hurt path does not reuse the idle clips.
    "skeleton": {
        "sounds/mob/skeleton/say1.ogg": "say1",
        "sounds/mob/skeleton/say2.ogg": "say2",
        "sounds/mob/skeleton/say3.ogg": "say3",
        "sounds/mob/skeleton/hurt1.ogg": "hurt1",
        "sounds/mob/skeleton/hurt2.ogg": "hurt2",
        "sounds/mob/skeleton/hurt3.ogg": "hurt3",
        "sounds/mob/skeleton/hurt4.ogg": "hurt4",
        "sounds/mob/skeleton/step1.ogg": "step1",
        "sounds/mob/skeleton/step2.ogg": "step2",
        "sounds/mob/skeleton/step3.ogg": "step3",
        "sounds/mob/skeleton/step4.ogg": "step4",
        "sounds/mob/skeleton/death.ogg": "death",
    },
    "villager": {
        "sounds/mob/villager/idle1.ogg": "idle1",
        "sounds/mob/villager/idle2.ogg": "idle2",
        "sounds/mob/villager/idle3.ogg": "idle3",
        "sounds/mob/villager/hit1.ogg": "hit1",
        "sounds/mob/villager/hit2.ogg": "hit2",
        "sounds/mob/villager/hit3.ogg": "hit3",
        "sounds/mob/villager/hit4.ogg": "hit4",
        "sounds/mob/villager/death.ogg": "death",
    },
    # Pig, sheep and creeper already have every clip they use in both tiers, so they are not listed.
    # Note that the tree's pig/hurt1-2 and creeper/hurt1-2 are not vanilla assets -- 1.7.3 has no
    # hurt clips for either mob and reuses their say clips -- so they cannot be regenerated from the
    # asset index and are deliberately left alone.
}

# asset tier on disk -> ffmpeg output format
TIERS = {
    "Sounds": ["-ac", "2", "-ar", "44100"],
    "SoundsLow": ["-ac", "1", "-ar", "22050"],
}


def asset_roots(game_root):
    """Every Assets/ directory that should receive the files (source tree + dist copy)."""
    roots = []
    for dirpath, dirnames, _ in os.walk(game_root):
        # don't descend into the Assets trees themselves
        if os.path.basename(dirpath) == "Assets":
            roots.append(os.path.dirname(dirpath))
            dirnames[:] = []
        elif dirpath.count(os.sep) > game_root.count(os.sep) + 3:
            dirnames[:] = []
    return sorted(set(roots))


def main():
    game_root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mobs", nargs="*", help="which mobs to import (see --list)")
    ap.add_argument("--all", action="store_true", help="import every mob in the table")
    ap.add_argument("--list", action="store_true", help="list the mobs and clips known to this script")
    ap.add_argument("--minecraft", default=os.path.join(os.environ.get("APPDATA", ""), ".minecraft"),
                    help="Minecraft data directory (default: %%APPDATA%%/.minecraft)")
    ap.add_argument("--index", default="1.7.3.json", help="asset index to read (default: 1.7.3.json)")
    ap.add_argument("--ffmpeg", default="ffmpeg", help="ffmpeg executable (default: ffmpeg on PATH)")
    args = ap.parse_args()

    if args.list:
        for mob, clips in sorted(MOBS.items()):
            print("%s:" % mob)
            for src, base in clips.items():
                print("   %-40s -> %s.wav" % (src, base))
        return 0

    wanted = sorted(MOBS) if args.all else args.mobs
    if not wanted:
        print("nothing to do: name a mob, or pass --all / --list")
        return 1
    unknown = [m for m in wanted if m not in MOBS]
    if unknown:
        raise SystemExit("unknown mob(s): %s (known: %s)" % (", ".join(unknown), ", ".join(sorted(MOBS))))

    index_path = os.path.join(args.minecraft, "assets", "indexes", args.index)
    objects = os.path.join(args.minecraft, "assets", "objects")
    if not os.path.exists(index_path):
        raise SystemExit("%s not found. Pass --minecraft pointing at your .minecraft." % index_path)
    index = json.load(open(index_path, "r", encoding="utf-8"))["objects"]

    roots = asset_roots(game_root)
    if not roots:
        raise SystemExit("no Assets/ directory found under %s" % game_root)
    print("writing into %d asset root(s):" % len(roots))
    for r in roots:
        print("   " + r)

    written = 0
    for mob in wanted:
        for src_key, base in MOBS[mob].items():
            entry = index.get(src_key)
            if entry is None:
                print("  SKIP %s (not in %s)" % (src_key, args.index))
                continue
            src = os.path.join(objects, entry["hash"][:2], entry["hash"])
            if not os.path.exists(src):
                print("  SKIP %s (asset object missing)" % src_key)
                continue
            for tier, fmt in TIERS.items():
                for root in roots:
                    out_dir = os.path.join(root, "Assets", tier, "mobs", mob)
                    os.makedirs(out_dir, exist_ok=True)
                    out = os.path.join(out_dir, base + ".wav")
                    cmd = [args.ffmpeg, "-y", "-loglevel", "error", "-i", src, *fmt,
                           "-sample_fmt", "s16", "-acodec", "pcm_s16le", out]
                    proc = subprocess.run(cmd, capture_output=True, text=True)
                    if proc.returncode != 0 or not os.path.exists(out):
                        raise SystemExit("ffmpeg failed for %s: %s" % (out, proc.stderr.strip()))
                    written += 1
        print("  %s: %d clip(s) per tier" % (mob, len(MOBS[mob])))

    print("\nwrote %d file(s)." % written)
    print("Nothing to copy into the distribution folder: Minecraft - Compiled/Assets is the same")
    print("tree as this one (they are linked), so the new WAVs are already on both sides.")
    print("Run tools/check-sound-loads.py to confirm every clip is in both tiers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
