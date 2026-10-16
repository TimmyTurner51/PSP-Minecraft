"""
Check the chicken's baked mesh + Chicken2.cpp draw offsets against vanilla ModelChicken.

The expected numbers are DERIVED from the `addBox` and `setRotationPoint` calls in
tools/vanilla-models/ModelChicken.java, not transcribed from the generated mesh. The derivation is
one line per box:

    mesh box centre = to_mesh(box centre in vanilla space - the body's box centre)
    mesh extents    = (D, H, W), or (H, D, W) for a bone vanilla rotates 90 degrees about X
    to_mesh(x, y, z) = (-z, -y, x)

to_mesh is the engine's axis convention: mesh X is fore-aft (+ forward), mesh Y is up, mesh Z is
lateral, and the mesh origin is the body's box centre. The body's 90 degree X rotation is why its
fore-aft length is its vanilla *depth*. Taking the body's box centre as the origin (rather than its
rotation point) is what makes the numbers land where the mesher's hand-tuned pig put them.

Then this script replays Chicken2::Render's transform for each buffer -- rotate, then translate,
because sceGum composes T*R -- and compares the result. So it catches drift in either the mesh or
the draw offsets.

Usage:  python tools/check-chicken-vanilla.py
"""
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
MESHES = os.path.join(ROOT, "Scripts", "CraftWorld_MobMeshes.cpp")
CHICKEN = os.path.join(ROOT, "Scripts", "Chicken2.cpp")
JAVA = os.path.join(HERE, "vanilla-models", "ModelChicken.java")

PIXEL = 1.0 / 16.0
B0 = 16                      # vanilla ModelChicken: byte b0 = 16
PI = math.pi

# straight out of ModelChicken.java: bone -> (setRotationPoint, [(textureOffset, addBox...), ...])
BONES = {
    "head": ((0.0, -1.0 + B0, -4.0), [((0, 0), (-2.0, -6.0, -2.0), (4, 6, 3))]),
    "bill": ((0.0, -1.0 + B0, -4.0), [((14, 0), (-2.0, -4.0, -4.0), (4, 2, 2))]),
    "chin": ((0.0, -1.0 + B0, -4.0), [((14, 4), (-1.0, -2.0, -3.0), (2, 2, 2))]),
    "body": ((0.0, B0, 0.0), [((0, 9), (-3.0, -4.0, -3.0), (6, 8, 6))]),
    "rightLeg": ((-2.0, 3.0 + B0, 1.0), [((26, 0), (-1.0, 0.0, -3.0), (3, 5, 3))]),
    "leftLeg": ((1.0, 3.0 + B0, 1.0), [((26, 0), (-1.0, 0.0, -3.0), (3, 5, 3))]),
    "rightWing": ((-4.0, -3.0 + B0, 0.0), [((24, 13), (0.0, 0.0, -3.0), (1, 4, 6))]),
    "leftWing": ((4.0, -3.0 + B0, 0.0), [((24, 13), (-1.0, 0.0, -3.0), (1, 4, 6))]),
}

# bones vanilla renders with a 90 degree X rotation (setRotationAngles: body.rotateAngleX = PI/2)
ROTATED_X = {"body"}

# buffers, the bones baked into each, and how Chicken2.cpp draws them
BUFFERS = [
    ("chickenBodyVertices", ["body"], [(0.0, 0.0, 0.0)], False),
    ("chickenHeadVertices", ["head", "bill", "chin"], [(4.0, 1.0, 0.0)], False),
    ("chickenLegVertices", ["rightLeg", "leftLeg"], [(0.5, -3.0, -1.5), (0.5, -3.0, 1.5)], False),
    ("chickenWingVertices", ["rightWing", "leftWing"], [(0.0, 3.0, 3.5), (0.0, 3.0, -3.5)], False),
]
TOL = 1e-4


def to_mesh(v):
    return (-v[2], -v[1], v[0])


def body_box_centre():
    pivot, boxes = BONES["body"]
    uv, corner, size = boxes[0]
    c = [pivot[i] + corner[i] + size[i] / 2.0 for i in range(3)]
    # the 90 degree X rotation is applied about the bone pivot, so rotate the centre about it too
    d = [c[i] - pivot[i] for i in range(3)]
    d = [d[0], -d[2], d[1]]
    return [pivot[i] + d[i] for i in range(3)]


def expected_world(bones, origin):
    """Vanilla-derived world extents, in mesh px, for a group of bones."""
    lo = [1e9] * 3
    hi = [-1e9] * 3
    for name in bones:
        pivot, boxes = BONES[name]
        for uv, corner, size in boxes:
            c = [pivot[i] + corner[i] + size[i] / 2.0 for i in range(3)]
            if name in ROTATED_X:
                d = [c[i] - pivot[i] for i in range(3)]
                d = [d[0], -d[2], d[1]]
                c = [pivot[i] + d[i] for i in range(3)]
                # a 90 degree X rotation tips the box over: its vanilla height runs fore-aft and
                # its vanilla depth runs vertically, so (dx, dy, dz) = (H, D, W)
                ext = [size[1], size[2], size[0]]
            else:
                ext = [size[2], size[1], size[0]]     # (dx, dy, dz) = (D, H, W)
            ctr = to_mesh([c[i] - origin[i] for i in range(3)])
            for i in range(3):
                lo[i] = min(lo[i], ctr[i] - ext[i] / 2.0)
                hi[i] = max(hi[i], ctr[i] + ext[i] / 2.0)
    return lo, hi


def parse_buffer(src, name):
    """Vertices of one generated buffer, in px."""
    start = src.index("/// * --- %s --- *" % name)
    end = src.index("%s = (TexturesPSPVertex*)memalign" % name, start)
    env = {"pixel": PIXEL, "textureXpixel": 1.0 / 64.0, "textureYpixel": 1.0 / 32.0,
           "x": 0.0, "y": 0.0, "z": 0.0, "left": 0.0, "right": 0.0, "down": 0.0, "up": 0.0}
    pts = []
    for line in src[start:end].splitlines():
        line = line.strip()
        m = re.match(r"^x = (.+?); y = (.+?); z = (.+?);$", line)
        if m:
            env["x"], env["y"], env["z"] = (eval(m.group(i), {"__builtins__": {}}, env) for i in (1, 2, 3))
            continue
        m = re.match(r"^(x|y|z) = (.+);$", line)
        if m:
            env[m.group(1)] = eval(m.group(2), {"__builtins__": {}}, env)
            continue
        m = re.match(r"^mPosition\.push_back\(new Vector3\((.+)\)\);$", line)
        if m:
            pts.append([eval(p, {"__builtins__": {}}, env) / PIXEL for p in m.group(1).split(",")])
    assert pts, "no vertices parsed for " + name
    return pts


def drawn_extents(pts, off, flip_pi):
    """Replay Render: sceGum composes T*R, so the rotation is applied to the vertex, then the
    translate. A PI rotation about Z negates x and y in the local frame."""
    lo = [1e9] * 3
    hi = [-1e9] * 3
    for p in pts:
        v = list(p)
        if flip_pi:
            v[0], v[1] = -v[0], -v[1]
        for i in range(3):
            v[i] += off[i]
            lo[i] = min(lo[i], v[i])
            hi[i] = max(hi[i], v[i])
    return lo, hi


def main():
    src = open(MESHES, encoding="utf-8", errors="replace").read()
    origin = body_box_centre()

    print("vanilla source : %s" % os.path.relpath(JAVA, ROOT))
    print("mesh origin    : the body box centre, vanilla (%.1f, %.1f, %.1f)" % tuple(origin))
    print()
    print("%-22s %-24s %-24s" % ("buffer", "drawn (mesh px)", "vanilla ModelChicken"))
    print("-" * 74)

    failures = 0
    for buf, bones, offsets, flip in BUFFERS:
        pts = parse_buffer(src, buf)
        elo, ehi = expected_world(bones, origin)

        # the union over however Render draws this buffer
        dlo = [1e9] * 3
        dhi = [-1e9] * 3
        if len(offsets) == 1:
            dlo, dhi = drawn_extents(pts, offsets[0], flip)
        else:
            # a mirrored pair drawn twice from one buffer: report the union of both draws
            for off in offsets:
                lo, hi = drawn_extents(pts, off, flip)
                for i in range(3):
                    dlo[i] = min(dlo[i], lo[i])
                    dhi[i] = max(dhi[i], hi[i])

        bad = [i for i in range(3) if abs(dlo[i] - elo[i]) > TOL or abs(dhi[i] - ehi[i]) > TOL]
        if bad:
            failures += 1
        print("%s %-20s X%+6.1f..%+6.1f Y%+6.1f..%+6.1f Z%+6.1f..%+6.1f  X%+6.1f..%+6.1f Y%+6.1f..%+6.1f Z%+6.1f..%+6.1f"
              % ("OK " if not bad else "BAD", buf,
                 dlo[0], dhi[0], dlo[1], dhi[1], dlo[2], dhi[2],
                 elo[0], ehi[0], elo[1], ehi[1], elo[2], ehi[2]))
        for i in bad:
            print("     axis %s off by %+.2f / %+.2f" % ("XYZ"[i], dlo[i] - elo[i], dhi[i] - ehi[i]))

    print("-" * 74)

    # the draw offsets themselves must also match Chicken2.cpp, or the check above is meaningless
    chk = open(CHICKEN, encoding="utf-8", errors="replace").read()
    for needed in ("PIXEL*4,PIXEL*1,0", "PIXEL*0.5f,-PIXEL*3", "PIXEL*3.5f", "PIXEL*3"):
        if needed not in chk.replace(" ", ""):
            print("WARNING: Chicken2.cpp no longer contains %r -- keep this check in sync" % needed)

    if failures:
        print("%d buffer(s) drift from the vanilla pivots" % failures)
        return 1
    print("PASS: every buffer lands exactly where ModelChicken.java's boxes and pivots put it.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
