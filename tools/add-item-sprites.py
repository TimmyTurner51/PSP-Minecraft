#!/usr/bin/env python3
"""
Composite an authentic 1.7.3 item sprite into the items.png atlas of every texture pack.

Adding an item means adding its art, and the art has to go in two places at once: the atlas cell the
item's constructor names, and every pack's copy of that atlas. Doing it by hand gets one of them
wrong -- the spawn egg shipped into `Texture Pack/` once and the creative tab showed an empty slot.

The sprite comes from the extracted 1.7.3 client under `_mc173_extract/`, never from a downloaded
image, so the art is the real thing.

The cell is given the way `Items.cpp` spells it:

    --texture-row 6 --plane 11      # CookedChicken

and note that `textureRow` counts up from the BOTTOM of the atlas -- CraftWorld::GetItemVerts
computes `startPixelsY = 256 - ((textureRow + 1) * 16)`. This script does that conversion, so the
numbers you pass are the numbers you write in the constructor.

    python tools/add-item-sprites.py --sprite feather --texture-row 2 --plane 7
    python tools/add-item-sprites.py --check          # report every declared cell
    python tools/add-item-sprites.py --copy           # also write to Minecraft - Compiled/

Refuses to overwrite art unless --force, so a wrong cell is reported rather than silently painted over.
"""

import argparse
import os
import re
import shutil
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)                      # .../PSP Minecraft Source Code
TEXTURES = os.path.join(ROOT, "Assets", "Textures")
ITEMS_CPP = os.path.join(ROOT, "Scripts", "Items.cpp")


def _find_up(name, start=TOOLS):
    """Walk up until a directory holding `name` is found. The extracted client lives beside the
    source tree rather than inside it, and how many levels up that is can change."""
    d = start
    for _ in range(6):
        if os.path.isdir(os.path.join(d, name)):
            return os.path.join(d, name)
        parent = os.path.dirname(d)
        if parent == d:
            break
        d = parent
    return os.path.join(os.path.dirname(ROOT), name)


EXTRACT = os.path.join(_find_up("_mc173_extract"), "assets", "minecraft", "textures", "items")
COMPILED = os.path.join(_find_up("Minecraft - Compiled"), "Assets", "Textures")

CELL = 16


def packs(base):
    if not os.path.isdir(base):
        return []
    return sorted(d for d in os.listdir(base) if os.path.isdir(os.path.join(base, d, "entity")))


def cell_origin(plane, texture_row, cell=CELL):
    """Top-left pixel of the cell for a (textureRow, plane) pair as Items.cpp states it."""
    return plane * cell, (CELL - 1 - texture_row) * cell


def cell_is_blank(im, x0, y0, cell=CELL):
    px = im.load()
    return all(px[x, y][3] == 0
               for y in range(y0, y0 + cell) for x in range(x0, x0 + cell))


def declared_cells():
    """[(class, textureRow, plane)] for every item constructor that names a cell."""
    src = open(ITEMS_CPP, encoding="utf-8", errors="replace").read()
    out = []
    for m in re.finditer(r"(\w+)::(\w+)\s*\(\s*\)\s*:\s*BaseItem\s*\(\s*\)\s*\{(.*?)\n\}", src, re.S):
        body = m.group(3)
        r = re.search(r"textureRow\s*=\s*(\d+)", body)
        p = re.search(r"\bplane\s*=\s*(\d+)", body)
        if r and p:
            out.append((m.group(1), int(r.group(1)), int(p.group(1))))
    return out


def check(base=TEXTURES):
    """Report cells that are declared but empty, and cells two items both claim."""
    from PIL import Image
    from collections import defaultdict
    print("checking %s" % base)
    claims = defaultdict(list)
    for cls, r, p in declared_cells():
        claims[(r, p)].append(cls)

    shared = {k: v for k, v in claims.items() if len(v) > 1}
    if shared:
        print("  %d cell(s) claimed by more than one item (they render the same art):" % len(shared))
        for (r, p), names in sorted(shared.items()):
            print("     textureRow=%-3d plane=%-3d  %s" % (r, p, ", ".join(names)))
    else:
        print("  no two items share a cell")

    blanks = []
    for pack in packs(base):
        path = os.path.join(base, pack, "items.png")
        if not os.path.isfile(path):
            continue
        im = Image.open(path).convert("RGBA")
        W, H = im.size
        if H // CELL != CELL:
            continue
        for cls, r, p in declared_cells():
            x0, y0 = cell_origin(p, r)
            if 0 <= x0 < W and 0 <= y0 < H and cell_is_blank(im, x0, y0):
                blanks.append((pack, cls, r, p))
    if blanks:
        print("  %d declared cell(s) are empty:" % len(blanks))
        for pack, cls, r, p in blanks:
            print("     %-16s %-22s textureRow=%-3d plane=%-3d" % (pack, cls, r, p))
    else:
        print("  every declared cell has art in every pack")
    return 1 if blanks or shared else 0


def add(sprite, row, plane, force, copy_compiled):
    from PIL import Image
    src_path = os.path.join(EXTRACT, sprite + ".png")
    if not os.path.isfile(src_path):
        raise SystemExit("no such sprite: %s\n  looked in %s" % (sprite, EXTRACT))
    sprite_im = Image.open(src_path).convert("RGBA").resize((CELL, CELL))
    x0, y0 = cell_origin(plane, row)
    print("sprite %s -> textureRow=%d plane=%d  = atlas pixel (%d,%d)" % (sprite, row, plane, x0, y0))

    bases = [("Assets", TEXTURES)]
    if copy_compiled:
        bases.append(("Compiled", COMPILED))

    wrote = skipped = 0
    for label, base in bases:
        for pack in packs(base):
            path = os.path.join(base, pack, "items.png")
            if not os.path.isfile(path):
                continue
            im = Image.open(path).convert("RGBA")
            if not cell_is_blank(im, x0, y0) and not force:
                print("  SKIP  %-8s %-16s cell is not empty (use --force to overwrite)" % (label, pack))
                skipped += 1
                continue
            # Clear the cell, then paste without a mask. Both steps matter: paste() *with* a mask
            # multiplies the source by its own alpha rather than blending over, which squares every
            # semi-transparent pixel's alpha, and alpha_composite() rewrites invisible pixels (alpha 0
            # but non-zero RGB) to fully zero. A plain paste replaces the cell verbatim.
            im.paste((0, 0, 0, 0), (x0, y0, x0 + CELL, y0 + CELL))
            im.paste(sprite_im, (x0, y0))
            im.save(path)
            wrote += 1
    print("  wrote %d atlas(es), skipped %d%s" % (wrote, skipped, "" if skipped else ""))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sprite", help="sprite name under _mc173_extract/.../items/ (no .png)")
    ap.add_argument("--texture-row", type=int, help="the textureRow to write in Items.cpp (bottom-up)")
    ap.add_argument("--plane", type=int, help="the plane to write in Items.cpp (the column)")
    ap.add_argument("--force", action="store_true", help="overwrite a cell that already has art")
    ap.add_argument("--copy", action="store_true", help="also write to Minecraft - Compiled/")
    ap.add_argument("--check", action="store_true", help="report empty and shared declared cells")
    args = ap.parse_args()

    if args.check:
        return check()
    if not (args.sprite and args.texture_row is not None and args.plane is not None):
        ap.error("need --sprite, --texture-row and --plane (or --check)")
    return add(args.sprite, args.texture_row, args.plane, args.force, args.copy)


if __name__ == "__main__":
    sys.exit(main())
