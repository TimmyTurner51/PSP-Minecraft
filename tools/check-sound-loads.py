"""
Audit SoundManager's LoadSfx paths: every sample the game asks for must exist in BOTH tiers.

This is the sound-side twin of check-texture-loads.py, and it guards the same kind of silent
failure. LoadSfx falls back to Assets/Sounds/ when a SoundsLow/ file is absent, so a missing
compact-tier sample does not crash and does not log -- the PSP-1000 just plays the wrong tier,
or, if the full-scale file is the one that is missing, returns NULL and hands pgeWavPlay a null
handle. That is how the cow went silent partway through a moo (see the note at the top of
make-vanilla-mob-sounds.py): nothing failed, it simply stopped making noise.

Run from anywhere:  python tools/check-sound-loads.py
"""
import os
import re

GAME_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SRC = os.path.join(GAME_ROOT, "Scripts", "SoundManager.cpp")

src = open(SRC, encoding="utf-8", errors="replace").read()

# Every LoadSfx target, in source order, with duplicates collapsed but the call count kept.
paths = re.findall(r'LoadSfx\(\s*"([^"]+)"\s*\)', src)
counted = {}
for p in paths:
    counted[p] = counted.get(p, 0) + 1

# The compact tier is the same path with Assets/Sounds/ -> Assets/SoundsLow/ (this is exactly the
# string surgery LoadSfx itself does, so the audit cannot drift from the lookup it is checking).
def low_tier(path):
    return path.replace("Assets/Sounds/", "Assets/SoundsLow/", 1)

print("%-52s %-9s %-9s %s" % ("sample", "full", "compact", "asked"))
print("-" * 84)

missing_full = []
missing_low = []
for p in sorted(counted):
    full = os.path.join(GAME_ROOT, p.replace("/", os.sep))
    low = os.path.join(GAME_ROOT, low_tier(p).replace("/", os.sep))
    ok_full = os.path.exists(full)
    ok_low = os.path.exists(low)
    if not ok_full:
        missing_full.append(p)
    if not ok_low:
        missing_low.append(p)
    print("%-52s %-9s %-9s %d" % (
        p.replace("Assets/Sounds/", ""),
        "ok" if ok_full else "MISSING",
        "ok" if ok_low else "MISSING",
        counted[p]))

print()
print("%d distinct sample(s) asked for." % len(counted))

if missing_full:
    print()
    print("MISSING FROM Assets/Sounds -- LoadSfx returns NULL for these, and pgeWavPlay is handed")
    print("a null handle (the path sound rather than a crash):")
    for p in missing_full:
        print("  %s" % p)

if missing_low:
    print()
    print("MISSING FROM Assets/SoundsLow -- these load fine on a 64 MB PSP and silently fall back")
    print("to the full tier on a 32 MB PSP-1000, which is the RAM the compact tier exists to save:")
    for p in missing_low:
        print("  %s" % low_tier(p))

if not missing_full and not missing_low:
    print("every LoadSfx sample is present in both tiers")
