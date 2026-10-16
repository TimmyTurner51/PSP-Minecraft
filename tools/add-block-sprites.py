"""
Composite an authentic 1.7.3 block sprite into the terrain atlas of every texture pack.

Why this exists
---------------
Blocks in this port do not load a PNG named after themselves. A block stores *atlas tile
coordinates* (`upPlane_x/y`, `leftPlane_x/y`, ... on BaseBlock) and the mesher turns those into UVs
inside the pack's `terrain.png`, using `percent = textureChunk / textureSize`, which
`CraftWorld::setTextureSize` fixes at 16/256 -- i.e. one sixteenth of the image per cell, whatever
the image's pixel size is. So giving a block a new look means painting a 16x16 cell into every pack's
atlas; there is no "drop a file in a folder" step, and `tools/add-item-sprites.py` only covers
items.png.

There are TWO atlases, and both must be painted:

    terrain.png        level 0/1. 256x256 in six packs, 512x512 in Natural (16 cells across either
                       way, so 16px and 32px cells respectively).
    terrain_mips.png   128x128 in ALL seven packs, wired in as mip level 2 by
                       TextureManager::SetMipMaps2Textures. 8px cells.

Measured, not assumed: terrain_mips.png is `terrain.png` resized to 128 with NEAREST -- that
reproduces 16166 of 16384 pixels of the existing file, the rest being drift from an older terrain.png
it was made from. So the mip cell for a staged sprite is the NEAREST downscale of what was staged.

That makes a new block's art the one place where a mistake is silent and destructive: overwrite a
cell another block points at and that block quietly changes appearance everywhere, with no error, and
the mip atlas has to be kept in step or the block shimmers to a different tile at distance. A *wrong*
cell is invisible in code review too -- the numbers are just integers.

So this script refuses to write into any cell Scripts/ references, reports what was in a cell before
it replaces it, will not write over art that looks like a block's own tiles without --force, and can
put a single cell back from the 2020 zip when a choice turns out to be wrong.

    python tools/add-block-sprites.py --check
        Report, for every cell this script knows about, whether each pack already holds the
        authentic 1.7.3 sprite, what else is there, and whether the port references the cell.

    python tools/add-block-sprites.py --piston
        Stage the four piston tiles into the cells the piston is registered with.

    python tools/add-block-sprites.py --piston --force
        Also overwrite a cell the port references (this WILL change an existing block).

    python tools/add-block-sprites.py --restore COL ROW
        Put one cell back the way the 2020 zip has it, in every pack and both atlases. This is the
        undo for a staging that turned out to be the wrong cell, and it is how (14,2) was put back.

The cells, and how they were chosen
-----------------------------------
The piston needs five tiles in vanilla: the pushing face of a normal and of a sticky piston, the
shared side, the back of the base, and the face at the bottom of the recess an extended base opens.
Four of them cost a cell, and three of those are vanilla's own spare slots:

    piston_top_normal (15,1)   piston_top_sticky (15,2)   piston_side (15,3)   piston_inner (15,14)

The fifth is satisfied without touching the atlas at all, and the refusal is deliberate:

  * **piston_bottom is furnace_top.** Again literally: md5 d1bd2cd57aeb26153432be7f0ab54101 for both
    files in the 1.7.3 jar. So the piston's back face reads (14,3), the furnace's top cell, and the
    pixels it shows ARE vanilla's piston_bottom -- while a themed pack, which themes that cell
    already, gets a themed piston back for free. Nothing is consumed. The block definition carries a
    comment, because this is the same coincidence that fooled the earlier pass and it should read as a
    decision, not an accident.

  * **piston_inner is seen, and it goes in a spare slot.** It is the face at the bottom of the four
    pixels an extended base leaves open -- stone, with a bright ring and a 4x4 hole drawn where the rod
    comes out of it -- and it is read from `innerPlane`, a field that exists for this one block. For a
    while this script staged nothing for it, on the reading that the head's opaque plate sat four
    pixels in front of that face and hid it. Both halves of that were wrong in the same way: the head
    is not four pixels away, it is a whole block away with the rod across the gap, and the base had no
    recessed face to hide in the first place, because the body that face belongs to was being drawn
    twelve pixels too short -- see `InitPistonBase`'s collideBox note. A piston in that state reads as
    two plates on a stick, and the middle of it, which is where this face lives, is the missing part.

    It was first staged at (14,2), on the reading that (14,2) was blank in all seven packs. It is not.
    (14,2) is painted in every pack, and in the two of them that carry vanilla's own art it is the
    classic **dispenser front** -- a stone face with the dispenser's mouth, 71 colours in Minecraft,
    nearer to 1.7.3's `dispenser_front_horizontal` than anything else in the jar (mean 2.4/255 per
    channel, and the residual is the packs predating 1.7.3's re-shade). This port has no dispenser
    *yet*: `changes.md` lists comparator / daylight sensor / observer / dispenser as later work. So
    that cell is art with an owner on the way, not a gap, and the piston moved to a real gap.

    (15,14) is that gap, and `spare_slot()` is what says so: a cell painted in *every* pack with the
    same flat grey pattern -- at most eight opaque pure-grey colours, byte for byte identical pack to
    pack -- is a slot vanilla never drew a texture for. (15,14) holds the 4x4-block grey pattern that
    also sits at (8,2), the one cell of this shape that anything in this port names (`ChestLock`
    points every face at it). (15,1)..(15,3) are the same kind of cell: transparent in five packs,
    flat marker colours in Painterly and Visibility, and drawn by nothing. Reading the slots as
    *blank* was the mistake that made (14,2) look free; the slots are painted, and that is the tell.

    The cell that looked free for this in the earlier pass, (15,8), is blank in all seven packs and yet
    is *not* free: `Redstoneoff` names it in its side and back plane fields. Blank is not the same as
    unclaimed, and staging there would have been the same shape of near-miss as the lit and unlit
    redstone torch cells this guard was rebuilt over -- a cell that looks empty, belongs to another
    block, and is silent about it. Believing the blank-and-unreferenced report from a scan that had not
    yet been taught about that reference is what the check below exists to stop.

    And the converse is the (14,2) mistake: *unclaimed* is not the same as *unused*. (14,2) is named
    by no block, no UV literal and no terrain-atlas item in this port, and it is still the dispenser's
    front face. A cell that nothing points at can be art that something is going to want. That is what
    `spare_slot()` is for: prefer the cell the atlas itself treats as a gap over any cell that is
    merely unpopular.

Do not "fix" the three that are listed to the vanilla-looking run at row 3 columns 11..15. That was
tried and it is wrong; an earlier pass concluded those five cells were the piston because
`piston_bottom.png` byte-matches what is at (14,3) -- which, per the point above, only proved that
(14,3) is the furnace top. Measured against the extracted sprites, this port's packs hold at that row:

    (11,3) crafting_table_side    (12,3) crafting_table_front
    (13,3) furnace_front_on       (14,3) furnace_top

and Blocks.cpp / CraftWorld_BlockPlanes.cpp actively point at all four (the furnace's front tile is
hardcoded there as `12*percent`, not read from a plane). Staging pistons there would have repainted
the crafting table and the furnace in every pack. The verification that "four of the five are already
authentic" was the same coincidence; --check compares exact pixel tuples, which is the only thing that
would have caught it.

And the three cells above are only trustworthy because the guard was rebuilt after it failed. Its
first version matched `upPlane_y = <digits>;`, and `InitRedstoneTorch` writes `upPlane_y = lit ? 6 : 7;`
-- so (3,6) and (3,7) were reported free, and piston_bottom and piston_inner were staged over the lit
and unlit redstone torch in all seven packs, turning every torch into a solid block. That was caught
by reading the block definitions rather than the script's output, and the cells were restored from the
2020 zip. `referenced_cells()` now scans whole expressions, knows that items with `terrainTexture =
true` address the terrain atlas too, and main() refuses to write a cell that is painted in *every*
pack without --force. Three nets, because one was not enough.

The (14,2) staging failed a different net in the same family and for the same reason: it was read as
blank, it was written with --force, and the only thing wrong with it was that nobody looked at what
the cell held before believing the blank report -- the same failure as the torches, one step further
out. So the guard now has a fourth test, `spare_slot()`, which is the positive form of the question
`blank_fraction` was answering: not "is this cell empty" but "is this cell's art the atlas's own
filler". The lesson worth keeping is not that a scan needs another pattern; it is that the atlas is
data, `_tmp_atlas/zip_orig/` holds the 2020 zip's copy of all fourteen files to compare against, and
`--restore` puts one cell of it back. Compare by ink, not by absence of ink.
"""
import argparse
import os
import re
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("needs pillow:  python -m pip install pillow")

TOOLS = os.path.dirname(os.path.abspath(__file__))
GAME_ROOT = os.path.dirname(TOOLS)


def _find_up(name, start=TOOLS):
    """Walk up until a directory holding `name` is found. The extracted client lives beside the source
    tree rather than inside it, and how many levels up that is can change -- add-item-sprites.py does
    the same thing, and hardcoding the depth here made --check fail with a bogus 'missing sprite'."""
    d = start
    for _ in range(6):
        if os.path.isdir(os.path.join(d, name)):
            return os.path.join(d, name)
        parent = os.path.dirname(d)
        if parent == d:
            break
        d = parent
    return os.path.join(os.path.dirname(GAME_ROOT), name)


EXTRACT = os.path.join(_find_up("_mc173_extract"), "assets", "minecraft", "textures", "blocks")
PACK_DIR = os.path.join(GAME_ROOT, "Assets", "Textures")
# The 2020 release zip, which is the only record of what a cell held before this script started
# writing into the atlas: `--restore` reads a cell back out of it. `_find_up` hands back the extracted
# client itself, so the zip is its sibling.
ZIP = os.path.join(os.path.dirname(_find_up("_mc173_extract")), "Source Code V3.0.0.zip")

# The cells the piston claims, and the 1.7.3 sprite that goes in each. Four, not five: see the module
# docstring -- piston_bottom is the furnace-top cell.
PISTON_CELLS = [
    ("piston_top_normal", 15, 1),   # the pushing face of a normal piston
    ("piston_top_sticky", 15, 2),   # the pushing face of a sticky piston
    ("piston_side",       15, 3),   # the four flanks of base and head, and the rod
    ("piston_inner",      15, 14),  # the face at the bottom of an extended base's recess
]


def packs():
    out = []
    for name in sorted(os.listdir(PACK_DIR)):
        if os.path.isfile(os.path.join(PACK_DIR, name, "terrain.png")):
            out.append(name)
    return out


def _bodies():
    """Every .cpp/.h under Scripts/, split on the same crude "one top-level '{' to the next lone '}'"
    boundary the rest of the checks use. Good enough for this file's style, and it is what makes a
    face whose x and y are assigned a few lines apart still pair up."""
    for root, _dirs, files in os.walk(os.path.join(GAME_ROOT, "Scripts")):
        for fn in files:
            if not fn.endswith((".cpp", ".h")):
                continue
            src = open(os.path.join(root, fn), encoding="utf-8", errors="replace").read()
            for body in re.split(r"\n\}\s*\n", src):
                yield os.path.join(root, fn), body


def referenced_cells():
    """Every (col,row) any Scripts/ file puts art in.

    Conservative on purpose: over-reporting costs us candidate cells; under-reporting would cost us
    someone else's texture -- which is not hypothetical. The first version of this function only
    matched `upPlane_y = <digits>;`, and `InitRedstoneTorch` writes its two tiles as

        b.upPlane_x = 3;
        b.upPlane_y = lit ? 6 : 7;

    so (3,6) and (3,7) were reported free and the piston's bottom and inner tiles were staged straight
    over the lit and unlit redstone torch in all seven packs. Three sources now, because the mesher
    and the block definitions have three:

      * plane assignments, paired per face and per function body. Each side's *expression* is scanned
        for integer literals and the two sets are crossed, so a ternary yields every cell it can name
        and `= 3` still yields exactly one.
      * `left = 12*percent;` hand-written UV literals -- CraftWorld_BlockPlanes.cpp special-cases the
        furnace's front tile this way, and a plane-only scan misses it. Any body containing two bare
        `N*percent` integers is taken as one (x,y) pair.
      * a repaint-in-every-pack warning in main(), which is the net for what neither of the above can
        see: a cell chosen from a table or a loop has no literals near the assignment at all, but it
        will still look like deliberate art rather than blank space.

    And a fourth source that is nothing to do with blocks: an *item* with `terrainTexture = true` is
    drawn with the terrain atlas, addressing it with its own `plane`/`textureRow` pair, which
    GetItemVerts turns into cell (plane, 15 - textureRow). RedstoneTorchItem is one, and its comment
    ("terrain row 6 = the lit redstone torch tile") is the second, independent witness that (3,6) was
    the torch. No scan of Block definitions can see these, so they are parsed out of Items.cpp here.
        """
    refs = set()
    for path, body in _bodies():
        per_face = {}
        for axis in ("x", "y"):
            for m in re.finditer(r"(\w+)Plane_%s\s*=\s*([^;]+);" % axis, body):
                values = set(int(n) for n in re.findall(r"\d+", m.group(2)))
                if values:
                    per_face.setdefault(m.group(1), {}).setdefault(axis, set()).update(values)
        for _face, v in per_face.items():
            for x in v.get("x", ()):
                for y in v.get("y", ()):
                    refs.add((x, y))

        literals = [int(m.group(1)) for m in re.finditer(r"=\s*(\d+)\s*\*\s*percent\s*;", body)]
        if len(literals) >= 2:
            refs.add((literals[0], literals[1]))

        if os.path.basename(path) == "Items.cpp" and re.search(r"terrainTexture\s*=\s*true", body):
            rows = set(int(n) for n in re.findall(r"textureRow\s*=\s*(\d+)", body))
            planes = set(int(n) for n in re.findall(r"plane\s*=\s*(\d+)", body))
            for p in planes:
                for r in rows:
                    refs.add((p, 15 - r))
    return refs


def blank_fraction(pack, col, row):
    atlas = atlas_of(pack, "terrain.png")
    px = list(cell_of(atlas, col, row).getdata())
    return sum(1 for p in px if p[3] == 0) / float(len(px))


def spare_slot(packs_, col, row):
    """Is this cell one of the atlas's own spare slots -- the atlas's filler, not art?

    Vanilla did not leave the slots it had no texture for transparent: it painted them with a flat
    grey pattern (4x4 blocks of four greys here, six colours in all). The piston's (15,1)..(15,3)
    and (15,14) are that; so is (8,2), the only one of them this port names. So "is the cell blank"
    is the wrong question -- (14,2) was *painted* and got read as free, and every cell that really is
    spare is painted too.

    The test is positive and needs all seven packs: at most eight distinct colours, every one of them
    opaque and pure grey (r == g == b), and the same ink in every pack. Real art fails it -- themed
    packs redraw a themed cell, they do not inherit it -- and the grey *art* that exists (bedrock,
    iron, coal) fails it in the packs that recolour it, so a hit is a cell nothing ever drew.
    """
    sigs = set()
    for pack in packs_:
        px = list(cell_of(atlas_of(pack, "terrain.png"), col, row)
                  .resize((16, 16), Image.NEAREST).getdata())
        if len(set(px)) > 8:
            return False
        if any(a != 255 or not (r == g == b) for r, g, b, a in set(px)):
            return False
        sigs.add(tuple(px))
    return len(sigs) == 1


def zip_atlas(pack, which):
    """The 2020 zip's copy of one pack's atlas, as an image. The zip holds two trees with the same
    pack names in them -- `Texture Pack/` and `Minecraft - Compiled/Assets/Textures/`, the live one --
    so the entry is picked by the `/Assets/Textures/` in its path rather than by its tail."""
    import zipfile
    with zipfile.ZipFile(ZIP) as z:
        for name in z.namelist():
            if "/Assets/Textures/" in name and name.endswith("/%s/%s" % (pack, which)):
                return Image.open(z.open(name)).convert("RGBA")
    sys.exit("no %s/%s in %s" % (pack, which, ZIP))


def restore_cell(col, row, packs_):
    """Put one cell back the way the zip has it, in every pack and both atlases."""
    print("restoring cell (%d,%d) from %s" % (col, row, os.path.basename(ZIP)))
    for pack in packs_:
        for which in ("terrain.png", "terrain_mips.png"):
            path = os.path.join(PACK_DIR, pack, which)
            live = Image.open(path).convert("RGBA")
            old = zip_atlas(pack, which)
            if old.size != live.size:
                sys.exit("%s/%s is %dx%d in the zip and %dx%d here"
                         % (pack, which, old.size[0], old.size[1], live.size[0], live.size[1]))
            tw = cell_size(live)
            before = describe(live, col, row)
            live.paste(old.crop((col * tw, row * tw, (col + 1) * tw, (row + 1) * tw)),
                       (col * tw, row * tw))
            live.save(path)
            print("  WROTE %-14s %-16s -- was %s" % (pack, which, before))


def list_free(packs_, refs):
    """Report the unreferenced cells, best first: spare slots first, then blank in the most packs.

    Not being referenced only means nothing draws the cell *today*. Three things can be true of one:
    it is the atlas's own filler (`spare_slot`, the only kind that costs nothing), it is blank in most
    packs, or it is art belonging to a block this port does not have -- which is what (14,2) turned
    out to be, the dispenser's front face. So the filler column is the one to read, and the blank
    count beside it is the weaker signal it always was."""
    rows = []
    for row in range(16):
        for col in range(16):
            if (col, row) in refs:
                continue
            fracs = dict((p, blank_fraction(p, col, row)) for p in packs_)
            blank = sum(1 for v in fracs.values() if v >= 1.0)
            rows.append((spare_slot(packs_, col, row), blank, min(fracs.values()),
                         col, row, fracs))
    rows.sort(key=lambda t: (not t[0], -t[1], -t[2], t[4], t[3]))
    print("unreferenced cells (%d of 256), spare slots first, then blank in the most packs:"
          % len(rows))
    print("%-9s %-6s %-7s %-8s %s" % ("cell", "spare", "blank", "min", "blank % per pack"))
    print("-" * 110)
    for spare, blank, mn, col, row, fracs in rows[:26]:
        print("(%2d,%2d)  %-6s %d/%-4d %5.0f%%   %s" % (col, row, "SPARE" if spare else "-",
              blank, len(packs_), 100 * mn,
              "  ".join("%s=%d%%" % (p[:4], 100 * fracs[p]) for p in packs_)))
    print()
    print("A SPARE slot is filler in every pack: vanilla never drew a texture for it, so staging there")
    print("costs nothing. Everything else is somebody's art -- blank in five packs means the themed")
    print("packs did not redraw that cell, not that the cell is going spare. Confirm by eye before")
    print("writing, and read (14,2) in the module docstring for how that goes wrong.")


def load_extract(name):
    path = os.path.join(EXTRACT, name + ".png")
    if not os.path.isfile(path):
        sys.exit("missing 1.7.3 sprite: %s" % path)
    return Image.open(path).convert("RGBA")


def atlas_of(pack, which):
    return Image.open(os.path.join(PACK_DIR, pack, which)).convert("RGBA")


def cell_size(atlas):
    return atlas.size[0] // 16


def cell_of(atlas, col, row):
    tw = cell_size(atlas)
    return atlas.crop((col * tw, row * tw, (col + 1) * tw, (row + 1) * tw))


def cell_is(atlas, col, row, ref):
    return list(cell_of(atlas, col, row).resize(ref.size, Image.NEAREST).getdata()) == list(ref.getdata())


def describe(atlas, col, row):
    px = list(cell_of(atlas, col, row).getdata())
    transparent = sum(1 for p in px if p[3] == 0)
    if transparent == len(px):
        return "blank"
    if transparent:
        return "%d%% blank" % (100 * transparent // len(px))
    return "%d colours" % len(set(px))


def stage(atlas, ref, col, row):
    tw = cell_size(atlas)
    sprite = ref if ref.size[0] == tw else ref.resize((tw, tw), Image.NEAREST)
    atlas.paste(sprite, (col * tw, row * tw))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="report state, change nothing")
    ap.add_argument("--free", action="store_true", help="list unreferenced cells, best first")
    ap.add_argument("--piston", action="store_true", help="stage the four piston tiles")
    ap.add_argument("--restore", nargs=2, type=int, metavar=("COL", "ROW"),
                    help="put one cell back from the 2020 zip, in every pack and both atlases")
    ap.add_argument("--force", action="store_true", help="allow overwriting a referenced cell")
    args = ap.parse_args()

    if not args.check and not args.piston and not args.free and args.restore is None:
        ap.error("pass --check, --free, --piston or --restore")

    refs = referenced_cells()
    allpacks = packs()
    targets = PISTON_CELLS

    if args.restore is not None:
        restore_cell(args.restore[0], args.restore[1], allpacks)
        print()
        print("done. Re-run with --check to confirm, then build: the mesher bakes these cells at chunk")
        print("build time, so a texture change needs a clean rebuild (or entering the world again).")
        return

    if args.free:
        list_free(allpacks, refs)
        return

    print("packs: %d   cells Scripts/ references: %d" % (len(allpacks), len(refs)))
    print()
    print("%-20s %-7s %-6s %-6s %s" % ("sprite", "cell", "ref'd", "mips", "state in each pack"))
    print("-" * 110)
    for name, col, row in targets:
        ref = load_extract(name)
        used = (col, row) in refs
        states = []
        mips_ok = 0
        for pack in allpacks:
            atlas = atlas_of(pack, "terrain.png")
            mips = atlas_of(pack, "terrain_mips.png")
            if cell_is(atlas, col, row, ref):
                states.append("%s=exact" % pack[:8])
            else:
                states.append("%s:%s" % (pack[:8], describe(atlas, col, row)))
            want = cell_of(atlas, col, row).resize((cell_size(mips), cell_size(mips)), Image.NEAREST)
            if list(cell_of(mips, col, row).getdata()) == list(want.getdata()):
                mips_ok += 1
        print("%-20s (%2d,%d)  %-6s %-6s %s" % (name.replace("piston_", ""), col, row,
                                                "YES" if used else "no",
                                                "%d/7" % mips_ok, "  ".join(states)))

    print()
    print("%-20s %-7s %-6s %-6s %s" % ("terrain_mips", "cell", "ref'd", "", "mip state in each pack"))
    print("-" * 110)
    for name, col, row in targets:
        ref = load_extract(name)
        states = []
        for pack in allpacks:
            mips = atlas_of(pack, "terrain_mips.png")
            mw = cell_size(mips)
            want = ref.resize((mw, mw), Image.NEAREST)
            states.append("%s=%s" % (pack[:8], "exact" if cell_is(mips, col, row, want) else describe(mips, col, row)))
        print("%-20s (%2d,%d)  %-6s %-6s %s" % (name.replace("piston_", ""), col, row,
                                                "YES" if (col, row) in refs else "no", "", "  ".join(states)))

    if args.check:
        print()
        clash = [(n, c, r) for n, c, r in targets if (c, r) in refs]
        # What still needs writing, and whether its cell looks like deliberate art rather than a spare
        # slot. Both of those are only warnings about *writing*: a cell that already holds the sprite we
        # want has been staged and says nothing about what it held before.
        pending, painted, spare = [], [], []
        for name, col, row in targets:
            ref = load_extract(name)
            left = [p for p in allpacks if not cell_is(atlas_of(p, "terrain.png"), col, row, ref)]
            if not left:
                continue
            pending.append((name, col, row, left))
            if spare_slot(allpacks, col, row):
                spare.append((name, col, row))
            elif all(blank_fraction(p, col, row) < 1.0 for p in allpacks):
                painted.append((name, col, row))
        if clash:
            print("NOTE: %d target cell(s) are referenced by a block. Staging would change that"
                  % len(clash))
            print("block's appearance. --force is required; prefer finding an unreferenced cell.")
        else:
            print("No target cell is referenced by any block, so staging cannot change an existing")
            print("block. The only art lost is in cells nothing points at.")
        if not pending:
            print()
            print("All %d targets are already staged in every pack and both atlases; nothing to write."
                  % len(targets))
        if spare:
            print()
            print("%d pending target(s) are spare slots -- filler in every pack, drawn by nothing, so"
                  % len(spare))
            print("staging costs no art: %s"
                  % " ".join("(%d,%d)" % (c, r) for _n, c, r in spare))
        if painted:
            print()
            print("WARNING: %d target cell(s) are painted in EVERY pack and are NOT the atlas's filler"
                  % len(painted))
            print("-- that is the shape of a real sprite, and (14,2) was lost exactly this way."
                  )
            print("Cells: %s" % " ".join("(%d,%d)" % (c, r) for _n, c, r in painted))
            print("Check each against the atlas by eye before staging; --force is required, and if the")
            print("sprite turns out to be a block's own tile the answer is a different cell, not --force.")
        return

    # Stage.
    print()
    for name, col, row in targets:
        ref = load_extract(name)
        # Spare slots are free; anything else painted in every pack is art, and needs a deliberate
        # --force. The distinction only matters if there is something to write: re-running this
        # staging must not start demanding --force for its own art.
        needs = [p for p in allpacks if not cell_is(atlas_of(p, "terrain.png"), col, row, ref)]
        free = bool(needs) and spare_slot(allpacks, col, row)
        every = bool(needs) and not free and all(blank_fraction(p, col, row) < 1.0 for p in allpacks)
        for pack in allpacks:
            for which in ("terrain.png", "terrain_mips.png"):
                path = os.path.join(PACK_DIR, pack, which)
                atlas = Image.open(path).convert("RGBA")
                want = ref if which == "terrain.png" else ref.resize((cell_size(atlas), cell_size(atlas)), Image.NEAREST)
                if cell_is(atlas, col, row, want):
                    print("  ok   %-20s (%d,%d) %-14s %-16s -- already staged" % (name, col, row, pack, which))
                    continue
                before = describe(atlas, col, row)
                if (col, row) in refs and not args.force:
                    print("  SKIP %-20s (%d,%d) %-14s %-16s -- referenced by a block; pass --force to overwrite"
                          % (name, col, row, pack, which))
                    continue
                if every and not args.force:
                    print("  SKIP %-20s (%d,%d) %-14s %-16s -- painted in every pack (%s); pass --force if that"
                          % (name, col, row, pack, which, before))
                    print("       sprite really is unused, but check it against a block's own tiles first")
                    continue
                stage(atlas, ref, col, row)
                atlas.save(path)
                print("  WROTE %-20s (%d,%d) %-14s %-16s -- was %s%s" % (name, col, row, pack, which, before,
                      " (a spare slot: filler in every pack)" if free else ""))

    print()
    print("done. Re-run with --check to confirm, then build: the mesher bakes these cells at chunk")
    print("build time, so a texture change needs a clean rebuild (or entering the world again).")


if __name__ == "__main__":
    main()
