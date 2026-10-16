#!/usr/bin/env python3
"""
Draw the PSP button prompt icons that the launcher prints inside its text.

The prompts used to spell the button out -- "press X" -- which is wrong twice over. It says "U" for a
key that does not exist on a PSP when it means Triangle, and even when the letter is right the eye has
to read it, where a shape is recognised without being read. Every game on the system draws the button
instead of naming it, so this makes the launcher match.

PIXEL ART, DRAWN ONE PIXEL AT A TIME
------------------------------------
Every shape here is a set of integer pixel coordinates and nothing else: no anti-aliasing, no
supersampling, no resampling on the way out. That is a deliberate reversal -- the first version of this
script drew at 8x and reduced, which is the right way to make a smooth circle and the wrong way to make
a 12-pixel one, where it produces soft edges that read as a smudge on a 480x272 screen. Circles at this
size should be obviously made of pixels.

The masks are built by testing pixel centres against a radius rather than by drawing arcs, because at
this size the result of an arc is a set of pixels anyway, and testing gives a shape that is symmetric by
construction.

The letters in the L and R tabs are 3x5 pixel glyphs defined below, not a font. A TrueType face at this
size anti-aliases into grey mush and cannot be made crisp, and the game's own font is not available to
this script. Three columns by five rows is the classic legible minimum and it is exact.

THE SHEET
---------
Six uniform cells, left to right:

    0 triangle    1 circle    2 cross    3 square    4 L    5 R

Uniform cells are the point. `Sprite(texture, startW, startH, endW, endH)` cuts a sub-rect out of one
texture, so a fixed cell size means the launcher can index a button as `cell * CELL` with no table of
rectangles to keep in sync with the art. CELL is 16 and the launcher draws a cell at 16, i.e. 1:1 with
nothing resampling it, which is what keeps the pixels square on screen.

The face buttons are round, matching the hardware. L and R are tabs rounded on ONE TOP CORNER ONLY --
the outer one, so L curves up and to the left and R up and to the right, and the pair reads as a
left-and-right. The other three corners of each are square. That is the shape the request asked for and
it is why the two are not simply a shared pill.

    python tools/make-button-icons.py             # write the sheet into every pack, and a preview
    python tools/make-button-icons.py --preview-only
"""

import argparse
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFont

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)                       # .../PSP Minecraft Source Code

SHEET_NAME = "buttons.png"

# The order the launcher indexes by. Kept here as the single source of truth for the layout, and
# repeated in the comment on the C++ side so the two cannot drift without someone noticing.
BUTTONS = ["triangle", "circle", "cross", "square", "L", "R"]

CELL = 16                       # the sheet's cell, in pixels -- and the size the launcher draws at

# Playstation button colours: the flat colours the shapes are printed in. What the eye is matching when
# it reads one of these at twelve pixels is the hue, not a gradient.
FACE = {
    "triangle": (85, 186, 87),
    "circle":   (222, 62, 62),
    "cross":    (58, 122, 214),
    "square":   (214, 85, 155),
}
PILL_FILL = (208, 208, 214)     # the triggers are grey, not coloured
INK = (255, 255, 255)
PILL_INK = (44, 44, 48)

# Geometry, in pixels. The disc is 12 across with a 12-pixel cell margin of 2, and the symbol inside it
# is 8 across, which is as much as will read inside a 12-pixel circle.
DISC_R = 6.0
SYM_R = 4.0

# The tab is even-sized (14x8), and so is the letter that goes on it (4x6).
#
# That matching parity is the whole point, and it is what was wrong before: the letter used to be 3x5,
# which is odd in both directions, while the tab was even. Then (tab - letter) is odd, and halving an odd
# number to centre something always loses half a pixel -- so the letter sat one pixel nearer the tab's
# left edge and one pixel nearer its top, which is exactly how L and R came to look off-centre inside
# their own buttons. Nothing about the tab.
#
# It does not have to be EVEN, it has to MATCH. But the tab wants to be even for a different reason: 14
# is the widest tab that leaves a margin in a 16-pixel cell, and 8 is what fits between the disc's 12
# without crowding it. So the letter is the one that moves.
PILL_W, PILL_H, PILL_R = 14, 8, 3

# Which pixels a rounded corner removes, by radius. Spelled out as art rather than computed, because at
# three pixels a float radius and a pixel radius are not the same question and this is the answer that
# looks right. Index 0 is the corner pixel itself; (1, 0) is the one beside it along the edge.
CORNER_CUT = {
    0: [],
    1: [],
    2: [(0, 0)],
    3: [(0, 0), (1, 0), (0, 1)],
    4: [(0, 0), (1, 0), (2, 0), (0, 1), (0, 2)],
}

# The L and R glyphs, as 3x5 pixel art. 'X' is ink.
GLYPHS = {
    "L": ["X...",
          "X...",
          "X...",
          "X...",
          "X...",
          "XXXX"],
    # Both glyphs have to USE every column they declare, not merely declare it. The centring below is
    # done on the glyph's declared width, so a letter with an empty right-hand column is measured 4 wide
    # and drawn 3 wide -- and lands half a pixel off, which is the very thing this size was chosen to
    # avoid. R reaches x=3 on three of its rows for that reason.
    "R": ["XXX.",
          "X..X",
          "XXX.",
          "X.X.",
          "X.X.",
          "X..X"],
}


# ---------------------------------------------------------------------------------------------
# Pixel masks. Everything below returns or takes a set of (x, y) integer coordinates.
# ---------------------------------------------------------------------------------------------

def circle(cx, cy, r):
    """A filled disc. The pixel centre is tested as (x, y) rather than (x + 0.5, y + 0.5) so a disc
    centred on a half-integer (7.5 in a 16-cell, for one) comes out symmetric."""
    pts = set()
    lo = int(cx - r) - 1
    hi = int(cx + r) + 2
    for y in range(lo, hi):
        for x in range(lo, hi):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                pts.add((x, y))
    return pts


def ring(cx, cy, r_out, thickness):
    """A ring: a disc with its middle taken out. One subtraction, so the two edges are concentric by
    construction rather than by two independent draws agreeing."""
    return circle(cx, cy, r_out) - circle(cx, cy, r_out - thickness)


def rect(x0, y0, x1, y1):
    """A filled rectangle, INCLUSIVE of both corners -- unlike range(), which is why this exists."""
    return {(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)}


def rect_outline(x0, y0, x1, y1, thickness):
    return rect(x0, y0, x1, y1) - rect(x0 + thickness, y0 + thickness, x1 - thickness, y1 - thickness)


def mirror_pair(cx, half):
    """The two x coordinates `half` either side of cx, as a symmetric integer pair.

    cx is a half-integer (7.5 in a 16-cell), so cx - half also lands on a half-integer and has to be
    pinned one way or the other: floor it, then MIRROR. Rounding the two sides independently is what
    makes an icon lean -- under banker's rounding 4.5 and 10.5 give 4 and 10, a centre of 7 rather than
    7.5, and that half pixel is visible at this size as a shape that sits off to one side of its disc.
    """
    left = int(math.floor(cx - half))
    return left, int(2 * cx) - left


def triangle(cx, y0, y1, half_max, thickness):
    """A hollow triangle, apex up.

    Built from one half-width per row and mirrored, so the symmetry is a property of the construction.
    The first version drew two sides and a base as three lines, which is the obvious way and is not
    symmetric: Bresenham steps one side a row earlier than the other and the apex comes out leaning.
    """
    pts = set()
    rows = y1 - y0
    for i in range(rows + 1):
        half = int(math.floor(i * half_max / float(rows) + 0.5))
        left, right = mirror_pair(cx, half)
        for d in range(thickness):
            pts.add((left + d, y0 + i))
            pts.add((right - d, y0 + i))
    base_l, base_r = mirror_pair(cx, half_max)
    for x in range(base_l, base_r + 1):
        pts.add((x, y1))                            # the base closes the shape
    return pts


def cross(cx, y0, y1, thickness):
    """An X, mirrored the same way. Two pixels thick, because one reads as a scratch at this size and the
    real glyph is a solid X."""
    pts = set()
    mid = (y0 + y1) / 2.0
    for y in range(y0, y1 + 1):
        left, right = mirror_pair(cx, abs(y - mid))
        for d in range(thickness):
            pts.add((left + d, y))
            pts.add((right - d, y))
    return pts


def tab(w, h, r, corner):
    """A w x h tab with ONE corner rounded: 'tl' or 'tr'. The other three stay square, which is the
    whole shape -- it is what makes L and R a recognisable pair rather than two grey pills."""
    cut = set(CORNER_CUT.get(r, []))
    pts = set()
    for y in range(h):
        for x in range(w):
            if corner == "tl" and (x, y) in cut:
                continue
            # Mirrored for the right-hand corner, by measuring from the other edge.
            if corner == "tr" and (w - 1 - x, y) in cut:
                continue
            pts.add((x, y))
    return pts


def centred(mask, cx, cy):
    """Moves a mask built around the origin so its centre lands on (cx, cy) in the cell."""
    xs = [p[0] for p in mask]
    ys = [p[1] for p in mask]
    dx = int(round(cx - (min(xs) + max(xs)) / 2.0))
    dy = int(round(cy - (min(ys) + max(ys)) / 2.0))
    return {(x + dx, y + dy) for (x, y) in mask}


# ---------------------------------------------------------------------------------------------
# The cells
# ---------------------------------------------------------------------------------------------

def stamp(img, mask, colour):
    px = img.load()
    for (x, y) in mask:
        if 0 <= x < img.width and 0 <= y < img.height:
            px[x, y] = colour + (255,) if len(colour) == 3 else colour


def face_cell(name):
    """One face button: a flat coloured disc with a white symbol on it, no outline and no rim. Flat is
    the request -- a rim at this size eats the two pixels the circle needs to be a circle."""
    img = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    c = CELL / 2.0 - 0.5            # 7.5 in a 16-cell: the centre of the middle four pixels

    stamp(img, circle(c, c, DISC_R), FACE[name])

    if name == "triangle":
        sym = triangle(c, 4, 11, 3, 2)
    elif name == "circle":
        sym = ring(c, c, SYM_R, 2)
    elif name == "cross":
        sym = cross(c, 4, 11, 2)
    else:  # square
        sym = rect_outline(4, 4, 11, 11, 2)

    # Clipped to the disc, so nothing pokes out of the coloured edge on a symbol that is a pixel too
    # wide. Cheaper and more robust than tuning each symbol to fit.
    stamp(img, sym & circle(c, c, DISC_R), INK)
    return img


def pill_cell(letter, corner):
    """One trigger: a grey tab with its outer top corner rounded and a 3x5 letter on it."""
    img = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))

    body = centred(tab(PILL_W, PILL_H, PILL_R, corner), (CELL - 1) / 2.0, (CELL - 1) / 2.0)
    stamp(img, body, PILL_FILL)

    glyph = GLYPHS[letter]
    gw = len(glyph[0])
    gh = len(glyph)

    # Centred on the TAB, not on the cell. The tab is what reads as the button, so the letter has to sit
    # in the middle of the tab rather than the middle of the 16-pixel cell it happens to be pasted into
    # -- and because the tab and the letter are both even-sized (see PILL_W above) that centring is exact
    # rather than half a pixel out. This is the other half of the fix: measuring from the cell left the
    # letter nearer the tab's left edge, and by a different amount again on the rounded corner's side.
    xs = [x for (x, y) in body]
    ys = [y for (x, y) in body]
    x0 = min(xs) + (max(xs) - min(xs) + 1 - gw) // 2
    y0 = min(ys) + (max(ys) - min(ys) + 1 - gh) // 2

    ink = {(x0 + x, y0 + y) for y in range(gh) for x in range(gw) if glyph[y][x] == "X"}

    # Clipped to the tab, so the letter can never spill over its edge however the metrics above change.
    stamp(img, ink & body, PILL_INK)
    return img


# ---------------------------------------------------------------------------------------------
# The sheet
# ---------------------------------------------------------------------------------------------

def build_sheet():
    sheet = Image.new("RGBA", (CELL * len(BUTTONS), CELL), (0, 0, 0, 0))
    for i, name in enumerate(BUTTONS):
        if name == "L":
            cell = pill_cell("L", "tl")
        elif name == "R":
            cell = pill_cell("R", "tr")
        else:
            cell = face_cell(name)
        sheet.paste(cell, (i * CELL, 0))
    return sheet


def packs():
    """Every texture pack's title/ directory.

    The sheet goes into all of them, not just the default. A pack chosen from the game's menu replaces
    the whole title/ directory, so a sheet that lived only in Minecraft/ would vanish the moment someone
    picked another pack -- and the prompts would quietly fall back to naming the buttons in words. This
    is the same reasoning add-item-sprites.py works under: art that belongs to the interface has to be
    in every pack, or it is in one pack and missing from the rest.

    Dotted directories are skipped, which is what keeps .svn out.
    """
    root = os.path.join(ROOT, "Assets", "Textures")
    found = []
    for name in sorted(os.listdir(root)):
        if name.startswith("."):
            continue
        title = os.path.join(root, name, "title")
        if os.path.isdir(title):
            found.append(title)
    return found


# ---------------------------------------------------------------------------------------------
# A picture of the result, because a 16-pixel icon is not judgeable on its own
# ---------------------------------------------------------------------------------------------

def build_preview(sheet):
    zoom = 7
    pad = 14
    line_h = CELL + 16
    width = max(sheet.width * zoom + pad * 2, 430)
    height = pad + line_h * 3 + 34

    # The launcher's changelog panel is 0x60000000 over dirt, which lands near this brown.
    img = Image.new("RGB", (width, height), (74, 52, 34))
    d = ImageDraw.Draw(img)

    try:
        small = ImageFont.load_default(size=10)
        label = ImageFont.load_default(size=13)
    except TypeError:
        small = label = ImageFont.load_default()

    y = pad
    d.text((pad, y), "at the size it is drawn (1x, one sheet pixel per screen pixel)", font=small,
           fill=(198, 184, 168))
    img.paste(sheet, (pad, y + 16), sheet)

    y += line_h
    d.text((pad, y), "enlarged {}x -- NEAREST, so the pixels are the pixels".format(zoom), font=small,
           fill=(198, 184, 168))
    big = sheet.resize((sheet.width * zoom, CELL * zoom), Image.NEAREST)
    img.paste(big, (pad, y + 16), big)

    y += line_h + 8
    d.text((pad, y), "in a prompt, the way the launcher centre-justifies it", font=small,
           fill=(198, 184, 168))

    ty = y + 20
    x = float(pad)
    for text, icon in (("Not installed - press ", None), ("", "cross"), (" to download it.", None)):
        if icon:
            k = BUTTONS.index(icon) * CELL
            icon_img = sheet.crop((k, 0, k + CELL, CELL))
            img.paste(icon_img, (int(x), ty - 5), icon_img)
            x += CELL + 2
        if text:
            d.text((x, ty), text, font=label, fill=(240, 240, 240))
            x += d.textlength(text, font=label)
    return img


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preview-only", action="store_true",
                    help="redraw the preview without rewriting the sheet into the packs")
    ap.add_argument("--out", default=None,
                    help="write to one path instead of every pack (for a quick look)")
    args = ap.parse_args()

    sheet = build_sheet()

    if not args.preview_only:
        if args.out:
            os.makedirs(os.path.dirname(args.out), exist_ok=True)
            sheet.save(args.out)
            print("wrote {}".format(args.out))
        else:
            for title in packs():
                target = os.path.join(title, SHEET_NAME)
                sheet.save(target)
                print("wrote {}".format(target))
            print("({} cells of {} px, {}x{})".format(len(BUTTONS), CELL, sheet.width, sheet.height))

    preview_path = os.path.join(TOOLS, "button-icons-preview.png")
    build_preview(sheet).save(preview_path)
    print("wrote {}".format(preview_path))
    return 0


if __name__ == "__main__":
    sys.exit(main())
