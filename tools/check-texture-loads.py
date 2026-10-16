"""
Audit TextureHelper: every texture a `GetTexture` case can ask for must be in the
`FastLoadTexture` lists, or `GetTextureNumber` returns -1 and the game dereferences
`Images[-1]` -- a wild pointer (that is the chicken-render crash).

Run from anywhere:  python _tmp_atlas/check_texture_loads.py
"""
import os
import re

SRC = os.path.join(
    r"c:\Users\minec\Documents\MC PSP Project",
    "Source Code V3.0.0", "PSP Minecraft Source Code", "Scripts", "TextureHelper.cpp")

src = open(SRC, encoding="utf-8", errors="replace").read()

# Textures an enum case can resolve to:  fileName = texturePatch + "....";
requested = {}
for m in re.finditer(r'case\s+(\w+)\s*:\s*\{?\s*fileName\s*=\s*texturePatch\s*\+\s*"([^"]+)"',
                     src, re.S):
    requested.setdefault(m.group(2), []).append(m.group(1))

loaded = set(re.findall(r'FastLoadTexture\(\s*"([^"]+)"\s*\)', src))

print("theoretical asset roots that are not individual files (always present in a pack):")
ALIASED = {"terrain.png", "terrain_mips.png"}

missing = {f: c for f, c in requested.items() if f not in loaded and f not in ALIASED}
print()
print("%-46s %s" % ("asset requested by GetTexture", "cases asking for it"))
print("-" * 90)
for f in sorted(requested):
    mark = "MISSING" if f in missing else "ok"
    print("%-46s %-8s %s" % (f, mark, ", ".join(sorted(requested[f]))))

print()
if missing:
    print("NOT LOADED -- GetTexture returns -1 for these, and SetTextureModeulate(-1) reads Images[-1]:")
    for f in sorted(missing):
        print("  %s  (case %s)" % (f, ", ".join(sorted(missing[f]))))
else:
    print("every requested asset is in a FastLoadTexture list")
