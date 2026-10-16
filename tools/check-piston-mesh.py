"""
Audit the piston mesher's orientation frame and its two sprite-window rules.

Why this exists
---------------
A piston is the one block family here whose mesh is hand-emitted from a table instead of built by
the shared plane builders (see the long comment above `blockModel == 17` in
CraftWorld_MeshBlocks.cpp). That buys a sprite that runs *along* the push direction, and it costs
two things that are invisible in the source and nearly invisible on the panel:

  1. The six-row (l, t, d) table is three tables of six small integers, so a wrong entry does not
     look wrong. It is only wrong in one of two ways, and both are quiet:
       * not a signed permutation -- e.g. +X read l = (0,1,1), which makes the frame non-orthonormal.
         A frame point (u, v, a) then lands on y = u + v, so an east-facing piston is a sheared slab
         sixteen pixels taller than its cell. A quarter of the pistons a player can place.
       * the *other* right-handed pair for the same d -- geometrically identical, since the boxes are
         symmetric across the push, and different only in how the art is turned.
  2. The windows are two numbers each, stated per box, and the only thing that says they are wrong is
     a rule in that same comment: no box may stretch the art, and the arm band must stay on the
     front-most 4 px of the whole assembly. A box whose window is eight sprite rows over twelve world
     pixels is a 1.5x squash along the whole base half of a rod, and nothing in the code says so.

Also audited here, because it is the same table being wrong in a third way: the anchor. The frame's
box is 0..16 on every axis, so its centre is frame (8, 8, 8) and it has to be anchored on the cell's
centre. Anchoring it on the cell's corner is also correct while every axis of (l, t, d) is positive,
which is three of the six facings, and emits the other three -- down, west, north -- wholly inside
the neighbouring block, the one the piston is mounted on.

All three of those were live on 2026-09-27 and all three are in the same twenty lines of code. The
renderer-based preview that found them (a toy camera drawing the real tiles) is _tmp_atlas/
piston_preview.py; this file is the part of it that is worth keeping, expressed against the source
so it cannot drift from it.

Checked:
  1. each of the six facings has l, t, d unit signed axes, mutually perpendicular, with l x t == d;
  2. the six d's are the six distinct directions, so no facing is a duplicate of another;
  3. makeVert anchors the box on the cell centre (the frame's - 8 offset), not on its corner;
  4. every *body* window -- the three boxes that are 16 px across -- is 1:1 with the world pixels its box
     covers along the push, so the art is not stretched, and the arm band only ever appears on the front
     -most 4 px of the assembly;
  5. the rod's four flanks wear the tile the other way round, which is what vanilla's piston_head says
     with rotation 90 on the rod's down face and 270 on its up face: the sprite's 16 *columns* run along
     the rod 1:1 and its 4 *rows* run across the rod's 4 px. Reading the columns across the rod, as
     every other box here does, is three grey stripes down a finger-width face; so is reading the window
     off the rod's own geometry, which is what pulled a 4 px rod in to the tile's middle four columns.
     The two halves are also one run of columns split at the cell boundary -- the head carries 12 -> 0
     and the base's stub 16 -> 12 -- so the band runs through the seam instead of stepping at it;
  6. the rod is 4x4, written 6..10 in both l and t, on its four flanks and on its tip cap;
  7. emitCap derives its cap UVs from the vertex's position *in the cell* (Wx/Wy/Wz), not from the
     raw frame coordinates, and a face whose normal is +Y takes its row from Z -- y does not vary
     across a horizontal face, so the obvious version samples one sprite row across the whole face;
  8. the head's back face wears the head tile and not the base's piston_bottom: vanilla's
     piston_head.json binds that face to #unsticky, which is block/piston_top;
  9. the collider's body thickness agrees with the body the mesh draws.

Run from anywhere:  python tools/check-piston-mesh.py
"""
import os
import re
import sys

GAME_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

# The sprite's top rows are the arm band and the twelve below them are the body. This is a property of
# the art as staged, which add-block-sprites.py --check owns; here it is the boundary the window rule
# is written against, and rule 5 is the one that depends on it.
BAND_ROWS = 4.0

MESH = "Scripts/CraftWorld_MeshBlocks.cpp"
WORLD = "Scripts/CraftWorld2.cpp"
BLOCKS = "Scripts/Blocks.cpp"

# Vanilla's piston_head rod is the element `from [6,6,4] to [10,10,16]`: 4 x 4 px, centred in the
# cell. The frame anchors a box's *corner* at 0 and at 16, so writing it as 4..12 is the same shape at
# twice the diameter, and every other check in this file passes at either width -- the extents, the cap
# rectangles and the window ratios are all just as legal at 8 px across. This one number is the only
# thing that can see it, which is why it is stated here and not left to the source comment.
ROD_U0, ROD_U1 = 6.0, 10.0
# And the other number no other check can see. Every box here wears the sprite's *rows* along the push
# and its 16 columns across, which for a 16 px box is 1:1 in both directions, so nothing is stretched
# and nothing says which way round the tile goes. The rod is 4 px across, and vanilla turns the tile a
# quarter for it -- rotation 90 on the rod's down face, 270 on its up face -- so its 16 columns run
# *along* the rod. That is the one box where the tile's long axis and the box's long axis are the same
# axis, and it is why the columns have to be checked here: read the rows along the rod instead, as
# every other box does, and the arm is three grey stripes; read the window off the rod's own geometry,
# which is what an earlier revision did, and it is one stripe down the middle.
ROD_COLS_PER_PX = 1.0

problems = []


def read(rel):
    return open(os.path.join(GAME_ROOT, rel), encoding="utf-8", errors="replace").read()


def table(src, name, n=6):
    m = re.search(r"kPiston%s\[\s*%d\s*\]\s*=\s*\{([^}]*)\}" % (name, n), src)
    if not m:
        problems.append("%s: kPiston%s[] not found" % (MESH, name))
        return None
    vals = [int(v) for v in re.findall(r"-?\d+", m.group(1))]
    if len(vals) != n:
        problems.append("%s: kPiston%s[] has %d entries, expected %d"
                        % (MESH, name, len(vals), n))
        return None
    return vals


def direction_table(src, name):
    m = re.search(r"kPiston%s\[\s*6\s*\]\s*=\s*\{([^}]*)\}" % name, src)
    if not m:
        problems.append("%s: kPiston%s[] not found" % (WORLD, name))
        return None
    return [int(v) for v in re.findall(r"-?\d+", m.group(1))]


def fx(lit):
    return float(lit.rstrip("f"))


def ev(expr, off=None):
    """Evaluate one argument of an emit call, at a given along-push offset.

    Two of the head's arguments are written as `12.0f + off` rather than as a number, because the
    same emission draws the resting head and the moving one; `off` is what tells them apart. The rod
    call names its end `rod` instead, and that name is resolved from the source's own definition --
    see STROKE_LOCALS -- rather than assumed here, so the invariants section 10 checks it against are
    checked against what the code actually computes.
    """
    e = re.sub(r"(\d(?:\.\d*)?)f\b", r"\1", expr.strip())
    if off is not None:
        for name, definition in STROKE_LOCALS.items():
            e = re.sub(r"\b%s\b" % name, "(%s)" % definition, e)
        e = re.sub(r"\boff\b", repr(float(off)), e)
    return float(eval(e, {"__builtins__": {}}, {}))


def ev_stroke(expr, progress):
    """Evaluate the head's offset as the base's stroke call writes it, at a given progress.

    This is the one number that says where the moving head is, and it is a function of the clock rather
    than of a literal, so it cannot be read off the source the way the rest of these windows can: the
    expression has to be evaluated at the two ends of the travel and asked where the head lands.
    """
    e = re.sub(r"(\d(?:\.\d*)?)f\b", r"\1", expr.strip())
    e = re.sub(r"\bstrokeProgress\b", repr(float(progress)), e)
    return float(eval(e, {"__builtins__": {}}, {}))


STROKE_LOCALS = {}


# ---------------------------------------------------------------- 1 and 2: the frame

mesh_src = read(MESH)
world_src = read(WORLD)

dirx = direction_table(world_src, "DirX")
diry = direction_table(world_src, "DirY")
dirz = direction_table(world_src, "DirZ")
latx, laty, latz = table(mesh_src, "LatX"), table(mesh_src, "LatY"), table(mesh_src, "LatZ")
tanx, tany, tanz = table(mesh_src, "TanX"), table(mesh_src, "TanY"), table(mesh_src, "TanZ")

faces = []
if None not in (dirx, diry, dirz, latx, laty, latz, tanx, tany, tanz):
    for f in range(6):
        faces.append((f,
                      (latx[f], laty[f], latz[f]),
                      (tanx[f], tany[f], tanz[f]),
                      (dirx[f], diry[f], dirz[f])))


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


print("%-14s %-11s %-11s %-11s %-6s %s" % ("facing", "l", "t", "d", "l.t", "l x t"))
print("-" * 74)
for f, l, t, d in faces:
    ok = (dot(l, l) == 1 and dot(t, t) == 1 and dot(d, d) == 1
          and dot(l, t) == 0 and dot(l, d) == 0 and dot(t, d) == 0)
    handed = cross(l, t) == d
    print("%-14s %-11s %-11s %-11s %-6d %s%s"
          % (("%d" % f), l, t, d, dot(l, t), cross(l, t),
             "" if ok else "   l, t and d must be unit axes"))
    if not ok:
        problems.append("facing %d: l=%s t=%s d=%s are not three mutually perpendicular unit axes"
                        % (f, l, t, d))
    if not handed:
        problems.append("facing %d: l x t is %s but d is %s -- the frame is left-handed or turned, "
                        "so a frame point does not land in the cell" % (f, cross(l, t), d))

ds = [d for _f, _l, _t, d in faces]
if faces and len(set(ds)) != 6:
    problems.append("the six facings do not have six distinct push directions: %s" % (ds,))

# ---------------------------------------------------------------- 3: the anchor

# Scoped to the piston region on purpose: the lever above it has a makeVert of its own, and a bare
# search for "auto makeVert" would audit that one instead.
piston_region = mesh_src[mesh_src.index("if (blockType->blockModel == 17 || blockType->blockModel == 18)"):
                         mesh_src.index("if (blockType->blockModel == 13)")]
make_vert = re.search(r"auto makeVert = \[&\].*?\n\t\t\};", piston_region, re.S)
if not make_vert:
    problems.append("%s: makeVert not found in the model 17/18 region" % MESH)
elif "- 8.0f" not in make_vert.group(0) or "0.5f" not in make_vert.group(0):
    problems.append(
        "%s: makeVert does not anchor the frame's (8, 8, 8) centre on the cell's centre. Without the "
        "- 8 the three facings whose axes are negative emit their whole shape into the neighbouring "
        "block" % MESH)

# ---------------------------------------------------------------- the model 17/18 region

start = mesh_src.index("if (blockType->blockModel == 17 || blockType->blockModel == 18)")
end = mesh_src.index("if (blockType->blockModel == 13)", start)
region = mesh_src[start:end]

i_head = region.index("if(isHead)")
i_ext = region.index("else if(extended)")
i_ret = region.index("else\n", i_ext)

# The head's box pair lives in a lambda, because the *stroke* draws it too -- from the base cell, slid
# back along the push -- and one emission shared by both is the only thing that can guarantee a moving
# head is the same shape as a resting one. Section 10 evaluates it at offsets; here it is off == 0.
head_lambda = re.search(r"auto emitHead = \[&\]\(([^)]*)\)\s*\{(.*?)\n\t\t\};", region, re.S)
if not head_lambda:
    problems.append("%s: no emitHead lambda in the model 17/18 region" % MESH)
head_src = head_lambda.group(2) if head_lambda else ""

# Any local the head's emission computes with, so `ev` can resolve it: as written, not as assumed. The
# `off` the lambda is called with is a parameter, and `ev` substitutes the offset for it directly.
for _name, _def in re.findall(r"const float (\w+) = ([^;]+);", head_src):
    STROKE_LOCALS[_name] = re.sub(r"(\d(?:\.\d*)?)f\b", r"\1", _def.strip())

shapes = [("head", head_src),
          ("extended", region[i_ext:i_ret]),
          ("retracted", region[i_ret:])]

FLANK = re.compile(r"emitFlanks\(\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,"
                   r"\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*\)")
ROD = re.compile(r"emitRodFlanks\(\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,"
                 r"\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*\)")
CAP = re.compile(r"emitCap\(\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*,"
                 r"\s*([^,()]+?)\s*,")

# ---------------------------------------------------------------- 4 and 5: the windows

print()
print("%-11s %-14s %9s %8s %8s %7s   %s"
      % ("shape", "along-push", "cross", "px", "rows", "rows/px", "arm band"))
print("-" * 80)
flank_count = 0
for shape, src_ in shapes:
    for m in FLANK.finditer(src_):
        u0, u1, v0, v1, a0, a1, sv_back, sv_front = [ev(g, 0.0) for g in m.groups()]
        flank_count += 1
        geom = abs(a1 - a0)
        rows = abs(sv_front - sv_back)
        ratio = (rows / geom) if geom else 0.0
        band = min(sv_back, sv_front) < BAND_ROWS

        # Every emitFlanks box is 16 px across now: the retracted cube, the extended base's body and the
        # head's plate. The rod does not come through here at all -- it is 4 px across and wears the
        # tile turned a quarter, which is what emitRodFlanks exists for -- so a narrow box reaching
        # emitFlanks is a rod back on the wrong mapping, and it is caught below rather than costumed.
        across = abs(u1 - u0) >= 16.0 and abs(v1 - v0) >= 16.0
        allowed_band = shape == "retracted" or (shape == "head" and a0 == 12.0)
        print("%-11s a %4.1f..%-4.1f %4.0fx%-4.0f %8.1f %8.1f %7.3f   %s"
              % (shape, a0, a1, abs(u1 - u0), abs(v1 - v0), geom, rows, ratio,
                 ("yes" + ("" if allowed_band else "  <-- WRONG PLACE")) if band else "no"))

        if not across:
            problems.append(
                "%s: emitFlanks is being called for a box that is %g x %g px across, not 16. Only the "
                "three full-width boxes go through it; the 4 px rod wears the tile a quarter turn round "
                "and belongs in emitRodFlanks, or its 16-column band is read across a 4 px face and the "
                "arm comes out as three grey stripes"
                % (shape, abs(u1 - u0), abs(v1 - v0)))
        if abs(ratio - 1.0) > 1e-6:
            problems.append(
                "%s: emitFlanks a %g..%g covers %g sprite rows over %g world pixels (%.3f rows per "
                "pixel), expected 1.0 -- the art is stretched along the push"
                % (shape, a0, a1, rows, geom, ratio))
        if band and not allowed_band:
            problems.append(
                "%s: emitFlanks a %g..%g reaches into sprite rows %g..%g, which is the arm band. Only "
                "the retracted cube's front 4 px and the head's plate (a 12..16) may wear it"
                % (shape, a0, a1, min(sv_back, sv_front), BAND_ROWS))
        if u0 < 0.0 or v0 < 0.0 or u1 > 16.0 or v1 > 16.0:
            problems.append("%s: emitFlanks across range (%g..%g, %g..%g) leaves the tile"
                            % (shape, u0, u1, v0, v1))
if flank_count != 3:
    problems.append("expected 3 emitFlanks calls in models 17/18 (retracted cube, extended body, head "
                    "plate), found %d" % flank_count)

# ---------------------------------------------------------------- 5: the rod's window, both halves

print()
print("the rod's window, where the tile turns a quarter: %g columns per world pixel along the arm,"
      % ROD_COLS_PER_PX)
print("and the tile's 4 rows across the rod's 4 px:")
rod_boxes = {}
rod_count = 0
for shape, src_ in shapes:
    for m in ROD.finditer(src_):
        l0, l1, t0, t1, a0, a1, u_back, u_front = [ev(g, 0.0) for g in m.groups()]
        rod_count += 1
        geom = abs(a1 - a0)
        cols = abs(u_front - u_back)
        ratio = (cols / geom) if geom else 0.0
        ok_ratio = abs(ratio - ROD_COLS_PER_PX) < 1e-6
        print("  %-10s a %4.1f..%-4.1f  l %4.1f..%-4.1f  t %4.1f..%-4.1f  columns %4.1f..%-4.1f over "
              "%5.1f px  ->  %.3f  %s"
              % (shape, a0, a1, l0, l1, t0, t1, u_back, u_front, geom, ratio,
                 "ok" if ok_ratio else "*** art stretched"))
        if not ok_ratio:
            problems.append(
                "%s: emitRodFlanks a %g..%g runs %g sprite columns over %g world pixels (%.3f columns "
                "per pixel), expected %g. The sprite's 16 columns are the arm's cross-section and map "
                "onto the arm's 16 px one for one; the two halves split that run at the cell boundary"
                % (shape, a0, a1, cols, geom, ratio, ROD_COLS_PER_PX))
        rod_boxes[shape] = (l0, l1, t0, t1, a0, a1, u_back, u_front)
if rod_count != 2:
    problems.append("expected 2 emitRodFlanks calls in models 17/18 (the head's rod and the extended "
                    "base's stub), found %d" % rod_count)

# The rod's across direction is the tile's four rows, and the four sv values emitRodFlanks writes are
# 0 and 4 -- a window that spans the band. Stated against the source because the numbers are inside the
# lambda, where a per-call regex cannot see them.
rod_body = re.search(r"auto emitRodFlanks = \[&\].*?\n\t\t\};", mesh_src, re.S)
if not rod_body:
    problems.append("%s: emitRodFlanks not found" % MESH)
else:
    body = rod_body.group(0)
    svs = set(re.findall(r",\s*([\d.]+)f\);", body))
    if svs != {"0.0", "4.0"}:
        problems.append(
            "%s: emitRodFlanks writes sv values %s, expected {0.0, 4.0}. The across window is the "
            "tile's four arm-band rows: fewer rows and the arm is flat, more and it shows body"
            % (MESH, sorted(svs)))
    if "uAt(" not in body:
        problems.append("%s: emitRodFlanks does not interpolate its columns along the arm" % MESH)

# ---------------------------------------------------------------- 6: the rod's 4x4 cross-section

print()
print("the rod's cross-section and the two halves' seam (vanilla: from [6,6,4] to [10,10,16]):")
if sorted(rod_boxes) != ["extended", "head"]:
    problems.append("expected exactly one rod box on the extended base and one on the head, found %s"
                    % sorted(rod_boxes))
else:
    for shape in ("extended", "head"):
        l0, l1, t0, t1 = rod_boxes[shape][:4]
        ok = abs(l0 - ROD_U0) < 1e-6 and abs(l1 - ROD_U1) < 1e-6 \
            and abs(t0 - ROD_U0) < 1e-6 and abs(t1 - ROD_U1) < 1e-6
        print("  %-10s l %4.1f..%-4.1f  t %4.1f..%-4.1f  %g x %g px  %s"
              % (shape, l0, l1, t0, t1, abs(l1 - l0), abs(t1 - t0),
                 "ok" if ok else "*** WRONG DIAMETER ***"))
        if not ok:
            problems.append(
                "%s: the rod's cross-section is l %g..%g x t %g..%g (%g x %g px). Vanilla's "
                "piston_head rod is from [6,6,4] to [10,10,16] -- %g x %g. The frame anchors a box's "
                "corner at 0 and 16, so a wider rod is geometrically legal and passes every other "
                "check here; it is visible only in the picture"
                % (shape, l0, l1, t0, t1, abs(l1 - l0), abs(t1 - t0),
                   ROD_U1 - ROD_U0, ROD_U1 - ROD_U0))

    # The two halves are one arm split across a cell boundary, so their column runs have to meet: the
    # head carries columns 12 -> 0 and the stub 16 -> 12. That is also what keeps the art 1:1 rather
    # than merely 1:1 on each piece -- a head running 0 -> 16 and a stub 0 -> 4 would pass the per-call
    # ratio check above and fold the band back on itself at the seam.
    e = rod_boxes["extended"]
    h = rod_boxes["head"]
    if abs(e[5] - 16.0) > 1e-6 or abs(h[4]) > 1e-6:
        problems.append("the extended rod is a %g..%g and the head's is a %g..%g, so they do not meet "
                        "at the cell boundary" % (e[4], e[5], h[4], h[5]))
    elif abs(e[7] - h[6]) > 1e-6:
        problems.append(
            "the rod's two halves disagree at the cell boundary: the extended stub ends on sprite "
            "column %g and the head's rod starts on %g, so the band steps by %g columns exactly where "
            "the two cells join" % (e[7], h[6], abs(e[7] - h[6])))
    else:
        lo = min(e[6], e[7], h[6], h[7])
        hi = max(e[6], e[7], h[6], h[7])
        print("  seam      extended ends column %g, head starts column %g; together %g..%g  %s"
              % (e[7], h[6], lo, hi,
                 "ok" if abs(lo) < 1e-6 and abs(hi - 16.0) < 1e-6 else "*** DOES NOT COVER THE TILE ***"))
        if abs(lo) > 1e-6 or abs(hi - 16.0) > 1e-6:
            problems.append(
                "the rod's two halves cover sprite columns %g..%g between them, but the arm's 16 px map "
                "onto the tile's 16 columns: the run has to be 0..16 or part of the band is squeezed "
                "into the piece that is left" % (lo, hi))

    # And the rod's tip cap has to match its flanks, or the rod's end is a different width to its side.
    for m in CAP.finditer(shapes[1][1]):
        cu0, cu1, cv0, cv1 = [ev(g, 0.0) for g in m.groups()[:4]]
        if abs(cu1 - cu0) < 16.0 or abs(cv1 - cv0) < 16.0:
            if abs(cu0 - ROD_U0) > 1e-6 or abs(cu1 - ROD_U1) > 1e-6 \
                    or abs(cv0 - ROD_U0) > 1e-6 or abs(cv1 - ROD_U1) > 1e-6:
                problems.append("the extended rod's tip cap spans u %g..%g x v %g..%g, which does not "
                                "match its %g x %g flanks" % (cu0, cu1, cv0, cv1,
                                                              ROD_U1 - ROD_U0, ROD_U1 - ROD_U0))

cap_expect = {"retracted": 2, "extended": 3, "head": 2}   # head: front+back plate caps; extended adds the rod tip
bad_caps = {s: len(CAP.findall(src_)) for s, src_ in shapes if len(CAP.findall(src_)) != cap_expect[s]}
if bad_caps:
    problems.append("emitCap counts per shape are %s, expected %s (head front/back, extended plate "
                    "front/back plus rod tip, retracted front/back) - a shape that is missing a cap is "
                    "see-through from along its push axis"
                    % (bad_caps, cap_expect))

# ---------------------------------------------------------------- 7: the caps

cap_body = re.search(r"auto emitCap = \[&\].*?\n\t\t\};", mesh_src, re.S)
if not cap_body:
    problems.append("%s: emitCap not found" % MESH)
else:
    body = cap_body.group(0)
    if not all(w in body for w in ("Wx", "Wy", "Wz")):
        problems.append(
            "%s: emitCap does not derive its UVs from the vertex's position in the cell. The frame "
            "coordinates are 0..16 along *signed* axes, so reading a UV straight off one lands "
            "outside the tile on the three facings whose axis is negative" % MESH)
    horiz = re.search(r"if\(ny != 0\.0f\)\s*\{(.*?)\}", body, re.S)
    if not horiz:
        problems.append("%s: emitCap has no separate ny != 0 case" % MESH)
    elif "Wy" in horiz.group(1):
        problems.append("%s: emitCap's horizontal case reads Wy, which is constant across the face, so "
                        "the whole face samples a single sprite row" % MESH)
    elif "Wz" not in horiz.group(1):
        problems.append(
            "%s: emitCap takes a horizontal face's row from something other than Z. y does not vary "
            "across a face whose normal is +/-Y, so the row has to come from the other horizontal "
            "axis, or the whole face samples a single sprite row" % MESH)
    vert = re.search(r"sv = 16\.0f - Wy;", body)
    if not vert:
        problems.append("%s: emitCap's vertical case no longer states sv = 16 - Wy" % MESH)

# ---------------------------------------------------------------- 8: the head's back face tile

# The base's back is vanilla's piston_bottom -- which is byte for byte furnace_top, so it shares the
# furnace's cell. The *head's* back is not: vanilla's piston_head.json binds its south face to
# "#unsticky" and both piston_head.json and piston_head_short.json define that as block/piston_top, so
# the ring of plate left visible around the rod wears the same tile as the pushing face. The mesher
# asks for `botPlane` on both backs, so the only thing that keeps this honest is InitPistonHead's
# definition -- a head whose botPlane was pointed at the base's cell would put furnace_top on the back
# of every head, which is a mistake this port has made once already.
blocks_src = read(BLOCKS)
init_head = re.search(r"static void InitPistonHead\(BaseBlock& b, bool sticky\)\s*\{(.*?)\n\}",
                      blocks_src, re.S)
if not init_head:
    problems.append("%s: InitPistonHead not found" % BLOCKS)
else:
    head_body = init_head.group(1)
    forfield = re.search(r"b\.forPlane_x = (\d+);\s*b\.forPlane_y = ([^;]+);", head_body)
    botfield = re.search(r"b\.botPlane_x = (\d+);\s*b\.botPlane_y = ([^;]+);", head_body)
    if not forfield or not botfield:
        problems.append("%s: InitPistonHead does not set both forPlane and botPlane" % BLOCKS)
    elif forfield.groups() != botfield.groups():
        problems.append(
            "InitPistonHead's botPlane is (%s, %s) but its forPlane is (%s, %s). The head's back face "
            "wears the head tile (vanilla's #unsticky = block/piston_top), not the base's "
            "piston_bottom; when they differ the back of every head shows furnace_top"
            % (botfield.group(1), botfield.group(2).strip(),
               forfield.group(1), forfield.group(2).strip()))
    else:
        print()
        print("the head's back face wears the head tile (%s, %s), as vanilla's #unsticky does"
              % (forfield.group(1), forfield.group(2).strip()))

# ---------------------------------------------------------------- 9: mesh against collider

body_thickness = re.search(r"const float f = ([\d.]+)f;", world_src)
body_mesh = None
for m in FLANK.finditer(shapes[1][1]):
    u0, u1, v0, v1, a0, a1 = [ev(m.group(i), 0.0) for i in range(1, 7)]
    if abs(a0) < 1e-6 and abs(u1 - u0) == 16.0 and abs(v1 - v0) == 16.0:
        body_mesh = (a1 - a0) / 16.0
if not body_thickness:
    problems.append("%s: PistonCellBox's body thickness not found" % WORLD)
elif body_mesh is None:
    problems.append("%s: no full 16x16 box in the extended base to compare the collider with" % MESH)
elif abs(body_mesh - fx(body_thickness.group(1))) > 1e-6:
    problems.append("the extended base's body is %.4f of a block in the mesh but %.4f in the "
                    "collider" % (body_mesh, fx(body_thickness.group(1))))
else:
    print("mesh and collider agree: the extended base's body is %g of a block thick in both"
          % body_mesh)

# ---------------------------------------------------------------- 10: the stroke

# The arm's travel. Java animates a piston with a TileEntityPiston and keeps the block ids still; this
# port moves the ids at once (collision and the redstone rules have to see the finished shape) and
# draws the travel *on top* of the moved world. The head is therefore emitted from the base cell, with
# an offset that runs from 0 -- the plate over the base's recess, which is the retracted cube's front
# 4 px -- to a whole cell of 16 px, where the resting mesher draws the head. Where the plate sits and
# where the rod ends are both functions of that offset, and neither is visible in a screenshot of a
# piston at rest.
print()
print("the stroke, which draws the head from the base cell a fraction of a cell along the push:")
STROKE_OFFS = (0.0, 4.0, 8.0, 12.0, 16.0)

stroke_time = re.search(r"kPistonStrokeTime = ([\d.]+)f", world_src)
if not stroke_time:
    problems.append("%s: kPistonStrokeTime not found" % WORLD)
elif abs(fx(stroke_time.group(1)) - 0.1) > 1e-6:
    problems.append("kPistonStrokeTime is %s, expected 0.1 -- Java's piston takes two ticks to travel, "
                    "and both of them are animated" % stroke_time.group(1))

for name, flag in (("ExtendPiston", "true"), ("RetractPiston", "false")):
    fn = re.search(r"bool CraftWorld::%s\(.*?\n\}" % name, world_src, re.S)
    call = re.search(r"StartPistonStroke\([^)]*\)", fn.group(0)) if fn else None
    if not call:
        problems.append("%s does not start a stroke, so the world moves and the arm does not"
                        % name)
    elif ("extending, %s" % flag) not in call.group(0) and (", %s)" % flag) not in call.group(0):
        problems.append("%s starts its stroke with the wrong direction: %s" % (name, call.group(0)))

frame = world_src[world_src.index("void CraftWorld::UpdateWorldProcesses"):]
frame = frame[:frame.index("void CraftWorld::UpdateWorldTime")]
if "UpdatePistonStrokes(dt)" not in frame:
    problems.append("%s: UpdateWorldProcesses does not advance the strokes" % WORLD)
elif frame.index("UpdatePistonStrokes(dt)") > frame.index("FlushRedstoneMeshes()"):
    problems.append("%s: UpdatePistonStrokes runs after FlushRedstoneMeshes, so every stroke is a frame "
                    "behind the pose it asked for" % WORLD)

if "ClearPistonStrokes()" not in read("Scripts/CraftWorld_SaveLoad.cpp"):
    problems.append("a save does not clear the strokes, which name absolute cells")

# The head cell must stand down while its base draws the pair, or the same head is drawn twice: once
# where it is and once where it is coming from.
head_branch = region[i_head:i_ext]
if "strokeHere == false" not in head_branch or "emitHead(0.0f" not in head_branch:
    problems.append("%s: the head branch does not stand down while a stroke is on it" % MESH)

# And the base has to draw the moving head, over the whole cell. `progress` of 0 has the plate covering
# the base's recess -- pixel for pixel the retracted cube, which is the shape the world has just left --
# and 1 has it a whole cell along, which is the head cell the resting mesher draws it in. A travel of
# (progress - 1) * 16, which is what this was until 2026-09-27, stops the plate at the base's own front
# wall and leaves the head to jump the last cell when the stroke ends: every pose was inside the two
# cells it owns and both end frames still rendered to a pixel-perfect match of the *wrong* pose's
# neighbourhood, so only asking where the head lands at progress 1 catches it.
base_tail = region[i_ret:]
stroke_call = re.search(r"emitHead\(\s*([^,]+?)\s*,\s*(true|false)\s*\)", base_tail)
if not stroke_call:
    problems.append("%s: the base does not draw the moving head. A stroke that draws nothing is a "
                    "piston that teleports its head a whole cell at a time" % MESH)
else:
    travel = [ev_stroke(stroke_call.group(1), p) for p in (0.0, 1.0)]
    if abs(travel[0]) > 1e-6 or abs(travel[1] - 16.0) > 1e-6:
        problems.append(
            "%s: the base draws the moving head at %g px with progress 0 and %g px with progress 1, "
            "expected 0 and 16. The head is drawn from the *base* cell, so its travel is the whole cell "
            "the resting mesher draws it a cell along; anything shorter snaps the head the rest of the "
            "way when the stroke ends" % (MESH, travel[0], travel[1]))
    if stroke_call.group(2) != "true":
        problems.append("%s: the base's stroke call is not flagged as a sliding head, so the plate's "
                        "back cap is emitted on the plane of the base's recess floor" % MESH)

# The two faces that land coplanar with one the base already draws, and are therefore left out for the
# length of a stroke. Both are one frame of a 0.1 s slide and both are a flicker of z-fighting when
# they are missed, which is exactly the class of defect a screenshot cannot catch.
if re.search(r"if\(sliding == false \|\| off > 0\.0f\)", head_src) is None:
    problems.append("%s: the head's back cap is not dropped for the pose where the plate's back face "
                    "lands on the base's recess floor. There the two quads are one plane" % MESH)
if re.search(r"if\(strokeHere == false\)\s*\{\s*\n\s*emitCap\(6\.0f, 10\.0f, 6\.0f, 10\.0f, 16\.0f",
             shapes[1][1]) is None:
    problems.append("%s: the base's rod tip cap is drawn during a stroke, which at the start of the "
                    "travel is coplanar with the plate that has closed over it" % MESH)

# And the numbers themselves. At rest (off == 0) both boxes have to be exactly what they were before
# there was a stroke at all -- the plate a 12..16 with the band's four rows, the rod a 0..12 wearing
# columns 12 -> 0 -- and at every other offset the plate and the rod have to move together, as one rigid
# 4 px plate on a 12 px rod: vanilla's piston_head model, translated. The rod is not a length that grows
# out of the base's mouth; telescoping it that way reads the same, because the length it gains is inside
# the base's body, but it reaches past the end of the sprite's own 16 columns.
head_flank = list(FLANK.finditer(head_src))
head_rod = list(ROD.finditer(head_src))
if len(head_flank) != 1 or len(head_rod) != 1:
    problems.append("the head's emission is %d plate call(s) and %d rod call(s), expected one of each"
                    % (len(head_flank), len(head_rod)))
else:
    print("   off    plate a        rows    rod a          columns       rod length")
    print("   " + "-" * 74)
    for off in STROKE_OFFS:
        p = [ev(g, off) for g in head_flank[0].groups()]
        r = [ev(g, off) for g in head_rod[0].groups()]
        p_a0, p_a1, p_back, p_front = p[4], p[5], p[6], p[7]
        r_a0, r_a1, r_back, r_front = r[4], r[5], r[6], r[7]
        print("  %5.0f   %5.1f..%-5.1f   %g..%-4g  %5.1f..%-6.1f  %5.1f..%-5.1f  %5.1f px"
              % (off, p_a0, p_a1, p_back, p_front, r_a0, r_a1, r_back, r_front, r_a1 - r_a0))

        if abs(p_a1 - p_a0 - 4.0) > 1e-6 or abs(p_back - 4.0) > 1e-6 or abs(p_front) > 1e-6:
            problems.append(
                "at off %g the plate is a %g..%g box wearing sprite rows %g..%g. The plate is rigid "
                "and 4 px thick, and it is the box the arm band is on: it has to stay rows 4 -> 0 over "
                "4 px at every offset" % (off, p_a0, p_a1, p_back, p_front))
        if abs(p_a0 - (12.0 + off)) > 1e-6:
            problems.append("at off %g the plate's back face is at %g, expected %g -- the plate has to "
                            "travel with the head" % (off, p_a0, 12.0 + off))
        if abs(r_a0 - off) > 1e-6:
            problems.append(
                "at off %g the rod starts at a %g instead of at %g. The rod is 12 px and rigid and "
                "travels with the plate, as vanilla's piston_head model does; anchored to the mouth it "
                "would be 28 px at the end of the travel and would sample the sprite past its own tile"
                % (off, r_a0, off))
        elif abs(r_a1 - (12.0 + off)) > 1e-6:
            problems.append("at off %g the rod ends at a %g, expected %g -- it has to reach the "
                            "plate's back face" % (off, r_a1, 12.0 + off))
        elif abs(r_back - 12.0) > 1e-6 or abs(r_front) > 1e-6:
            problems.append(
                "at off %g the rod runs sprite columns %g -> %g. Column 0 is the plate's back face and "
                "column 12 is where the base's stub takes over, so the arm's bands stay continuous "
                "across the cell boundary and the sprite is never sampled outside its own tile"
                % (off, r_back, r_front))
        if r_a1 > r_a0:
            cols = abs(r_front - r_back)
            if abs(cols / (r_a1 - r_a0) - ROD_COLS_PER_PX) > 1e-6:
                problems.append("at off %g the rod runs %g columns over %g px, expected %g per pixel"
                                % (off, cols, r_a1 - r_a0, ROD_COLS_PER_PX))

stroke_body = re.search(r"void CraftWorld::UpdatePistonStrokes.*?\n\}", world_src, re.S)
if not stroke_body or "baseOpaque" not in stroke_body.group(0) or "extending == 0" not in stroke_body.group(0):
    problems.append("%s: UpdatePistonStrokes does not pick the mesh pass by direction. An extending "
                    "piston draws a transparent body and a retracting one draws an opaque cube, and the "
                    "walk that is skipped is the pass the id did not move in" % WORLD)
if "IsPistonBaseBlock(GetBlock(bx,by,bz)) == false" not in world_src:
    problems.append("%s: GetPistonStroke does not check that a head's base is still a base. A head "
                    "left standing on its own by a damaged save would then be drawn by nobody" % WORLD)

print()
if problems:
    print("PROBLEMS:")
    for p in problems:
        print("  %s" % p)
    print()
    print("%d problem(s) found" % len(problems))
    sys.exit(1)

print("every facing's frame is a right-handed orthonormal frame anchored on the cell centre, the rod is")
print("4x4 and wears the arm band the way round vanilla turns it, and the mesh agrees with the collider")
