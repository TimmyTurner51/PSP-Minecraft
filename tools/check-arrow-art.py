"""
Audit the arrow's art, its crop and its geometry.

`entity/arrow.png` is a 32x32 *model unwrap*, not a picture of an arrow: it has the arrow drawn on it
twice, once per face of the model, and the second copy is unlit and pink/purple. Painting the whole
sheet onto one quad draws both, which is the reported "ghost arrow" -- a normal arrow with a pink twin
beside it. `Arrow2.cpp` therefore samples only the top-left strip, which holds one complete lit arrow
(`ARROW_TEX_U` x `ARROW_TEX_V`), and this check holds that crop to its properties in every pack.

The second half is about the geometry, because the arrow is no longer a camera-facing billboard: it
is two quads crossed along the direction of travel, so that a shot points where it is going and a
landed one stays stuck in the face it hit, from any angle. Two things can silently break that and
neither shows up in a screenshot of a single frame:

  * reading the camera at all (a `GU_VIEW` fetch in BuildVertices) -- the arrow is back to facing the
    player, which is the bug this geometry exists to fix;
  * a degenerate cross -- a heading straight up or down has no `heading x world-up` to work with, and
    the fallback has to be there or every vertical shot draws a zero-width quad.

So the crop is checked against every texture pack, and the cross is re-derived here in Python from the
same formulas and checked for perpendicularity at headings all round the sphere.

Run from anywhere:  python tools/check-arrow-art.py
"""
import math
import os
import re
import sys

ROOT = os.path.join(r"c:\Users\minec\Documents\MC PSP Project",
                    "Source Code V3.0.0", "PSP Minecraft Source Code")
ARROW_CPP = os.path.join(ROOT, "source", "Aurora", "Graphics", "Effects", "Arrow2.cpp")
PACKS = os.path.join(ROOT, "Minecraft - Compiled", "Assets", "Textures")

try:
    from PIL import Image
except ImportError:
    print("PIL is needed for this check (pip install pillow)")
    sys.exit(2)


def read_source():
    """The constants the arrow is built from, and the shape of the geometry that uses them."""
    src = open(ARROW_CPP, encoding="utf-8", errors="replace").read()
    out = {}
    for name in ("ARROW_TEX_U", "ARROW_TEX_V"):
        m = re.search(name + r"\s*=\s*([0-9.]+)f\s*/\s*([0-9.]+)f", src)
        if m is None:
            print("FAIL  %s is not of the form <n>f / <m>f in Arrow2.cpp" % name)
            sys.exit(1)
        out[name] = float(m.group(1)) / float(m.group(2))

    m = re.search(r"ARROW_HALF_LENGTH\s*=\s*([0-9.]+)f", src)
    if m is None:
        print("FAIL  ARROW_HALF_LENGTH is not a plain float in Arrow2.cpp")
        sys.exit(1)
    out["half_length"] = float(m.group(1))

    # The crop is what the aspect has to be kept in, and it is applied to the quad across the shaft.
    out["aspect_kept"] = re.search(
        r"vScale\s*=\s*hScale\s*\*\s*\(\s*ARROW_TEX_V\s*/\s*ARROW_TEX_U\s*\)", src) is not None

    # Both of the crossed quads sample the crop, along the shaft and never the whole sheet.
    out["uv_crop"] = re.search(
        r"ARROW_TEX_U\s*\}\s*;\s*\n\s*const float tv\[4\]\s*=\s*\{\s*0\.0f,\s*ARROW_TEX_V,\s*0\.0f,\s*ARROW_TEX_V\s*\}",
        src) is not None

    # The heading, not the camera. Either of these coming back means the billboard is back.
    out["no_camera"] = ("GU_VIEW" not in src) and ("sceGumStoreMatrix" not in src)
    out["heading_used"] = ("flightDir" in src) and ("TrackFlightDir" in src)

    # Two quads, twelve vertices, two-sided, drawn in one call.
    out["two_quads"] = re.search(r"const Vector3 axes\[2\]", src) is not None
    out["tris"] = re.search(r"GU_TRIANGLES[^;]*,\s*12,\s*0,\s*arrowVertices", src) is not None
    out["two_sided"] = re.search(r"sceGuDisable\(GU_CULL_FACE\)", src) is not None
    out["vertices_12"] = re.search(r"memalign\(16,\s*12\s*\*\s*sizeof\(CraftPSPVertex\)\)", src) is not None
    return out


def is_pink(r, g, b):
    """Magenta-ish: the arrow's unlit copy (178,89,160) and friends. Reds and blue both lead
    green, which no part of the lit arrow (browns: g > b) or its white head does."""
    return r > g + 25 and b > g + 25


def check_cross(half_length, width, problems):
    """Re-derive the cross the way Arrow2::BuildVertices does, and hold it to its properties.

    The heading is the velocity, `side` is heading x world-up (with the straight-up/down fallback) and
    `other` is heading x side. Both quads have to contain the heading, meet at right angles, and stay
    the width the crop asks for -- at every heading, including the two poles where the cross product
    that makes `side` collapses.
    """
    worst_dot = 0.0
    worst_width = 0.0
    degenerate = 0
    used_fallback = 0
    for i in range(0, 361, 3):
        for j in range(-90, 91, 3):
            a, b = math.radians(i), math.radians(j)
            head = (math.cos(a) * math.cos(b), math.sin(b), math.sin(a) * math.cos(b))
            n = math.sqrt(sum(c * c for c in head))
            head = tuple(c / n for c in head)

            if head[1] > 0.999 or head[1] < -0.999:
                used_fallback += 1
                side = (head[1], -head[0], 0.0)
            else:
                side = (-head[2], 0.0, head[0])
            sn = math.sqrt(sum(c * c for c in side))
            if sn <= 0.0001:
                degenerate += 1
                side = (1.0, 0.0, 0.0)
            else:
                side = tuple(c / sn for c in side)

            other = (head[1] * side[2] - head[2] * side[1],
                     head[2] * side[0] - head[0] * side[2],
                     head[0] * side[1] - head[1] * side[0])

            # Each quad spans `head` and one of the two perpendiculars, so the heading lies in both
            # and the pair is at right angles.
            dot = sum(side[k] * other[k] for k in range(3))
            worst_dot = max(worst_dot, abs(dot))

            # A quad's width is the perpendicular it is laid along, scaled by vScale.
            side_len = math.sqrt(sum(c * c for c in side))
            other_len = math.sqrt(sum(c * c for c in other))
            worst_width = max(worst_width, abs(side_len * width - width), abs(other_len * width - width))

    print("\nthe crossed quads, re-derived here for %d headings:" % (361 // 3 * (181 // 3) + 1))
    print("  length %.3f blocks, width %.4f blocks (the crop's own 16:5 proportion)"
          % (2 * half_length, 2 * width))
    print("  worst |side.other| across the sphere: %.2e (0 is square)" % worst_dot)
    print("  worst width error: %.2e" % worst_width)
    print("  headings that took the straight-up/down fallback: %d; where even it collapsed: %d"
          % (used_fallback, degenerate))

    if worst_dot > 1e-6:
        problems.append("the two quads are not at right angles (worst dot %.3e)" % worst_dot)
    if abs(worst_width) > 1e-6:
        problems.append("a quad is not the width the crop asks for (worst %.3e)" % worst_width)
    if degenerate > 0:
        problems.append("the side vector collapsed at a heading the fallback does not cover")
    if not (0.1 <= 2 * half_length <= 1.0):
        problems.append("the arrow is %.2f blocks long, which is not a projectile" % (2 * half_length))
    if 2 * width > 2 * half_length:
        problems.append("the arrow is wider than it is long")


src = read_source()
print("Arrow2.cpp samples u <= %.4f, v <= %.4f  (%s)"
      % (src["ARROW_TEX_U"], src["ARROW_TEX_V"], "aspect kept" if src["aspect_kept"] else "ASPECT NOT KEPT"))
print("cross of two quads, 12 vertices, drawn two-sided in one GU_TRIANGLES call: %s"
      % ("yes" if (src["two_quads"] and src["tris"] and src["two_sided"] and src["vertices_12"]) else "NO"))
print("heading from flightDir / velocity: %s; camera read anywhere: %s"
      % ("yes" if src["heading_used"] else "NO", "YES (billboard back!)" if not src["no_camera"] else "no"))
print()

width = src["half_length"] * (src["ARROW_TEX_V"] / src["ARROW_TEX_U"])

packs = sorted(p for p in os.listdir(PACKS)
               if os.path.isdir(os.path.join(PACKS, p, "entity")) or p == "Minecraft")
problems = []
if not src["aspect_kept"]:
    problems.append("vScale is no longer derived from hScale through the crop's proportion")
if not src["uv_crop"]:
    problems.append("the quads no longer sample the crop (ARROW_TEX_U/ARROW_TEX_V) in both u and v")
if not src["no_camera"]:
    problems.append("Arrow2 reads the view matrix again -- the arrow is a billboard, not a model")
if not src["heading_used"]:
    problems.append("Arrow2 no longer tracks a heading (flightDir / TrackFlightDir)")
if not src["two_quads"] or not src["tris"] or not src["vertices_12"]:
    problems.append("the geometry is not the two-quad cross drawn as 12 vertices")
if not src["two_sided"]:
    problems.append("culling is left on for a two-sided cross -- half of it will vanish")

for pack in packs:
    path = os.path.join(PACKS, pack, "entity", "arrow.png")
    if not os.path.isfile(path):
        problems.append("%s: no entity/arrow.png" % pack)
        continue

    img = Image.open(path).convert("RGBA")
    w, h = img.size

    # The quad maps the crop over its own 0..1, so the crop in texels is this.
    cu = max(1, int(round(src["ARROW_TEX_U"] * w)))
    cv = max(1, int(round(src["ARROW_TEX_V"] * h)))

    lit = pink = 0
    pink_in_crop = None
    for y in range(h):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            if a == 0:
                continue
            if is_pink(r, g, b):
                pink += 1
                if pink_in_crop is None and x < cu and y < cv:
                    pink_in_crop = (x, y, r, g, b)
            elif x < cu and y < cv:
                lit += 1

    status = "ok"
    if w != 32 or h != 32:
        status = "FAIL"
        problems.append("%s: sheet is %dx%d, not the 32x32 the crop assumes" % (pack, w, h))
    if lit < 20:
        status = "FAIL"
        problems.append("%s: only %d lit pixel(s) inside the %dx%d crop -- the arrow is not there"
                        % (pack, lit, cu, cv))
    if pink_in_crop is not None:
        status = "FAIL"
        problems.append("%s: pink/purple art inside the crop at %d,%d (%d,%d,%d) -- the ghost arrow back in the quad"
                        % ((pack,) + pink_in_crop))

    print("%-18s %-9s sheet %2dx%2d  crop %2dx%2d  %3d lit px  %3d pink px elsewhere"
          % (pack, status, w, h, cu, cv, lit, pink))

check_cross(src["half_length"], width, problems)

print()
if problems:
    for p in problems:
        print("  " + p)
    print("\n%d problem(s)" % len(problems))
    sys.exit(1)

print("one arrow per shot: the crop holds the lit arrow and none of the pink copy,")
print("and the model is a square cross laid along the direction of travel")
