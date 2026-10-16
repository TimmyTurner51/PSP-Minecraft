#!/usr/bin/env python3
"""
Audit redstone wire's pick shape: the crosshair must pass over a wire lying in front of the player,
and it must be able to land on one.

Why this exists
---------------
Wire is 1 px tall. A player building a line outward -- standing at one end and placing each cell a step
further on -- aims at a *floor* cell and never at the wire itself, so the crosshair has to be able to
see past the wire already lying in front of their feet. Java gets this from
`BlockRedstoneWire.setBlockBounds(0, 0, 0, 1, 0.0625, 1)` plus two separate shapes per block: the 1 px
box is both the collision shape (which is never consulted, because wire is not solid) and the *selection*
shape the ray trace reads.

This port has one box per block, so the selection shape is where the wire's 1 px has to be answered. It
was not answered at all -- `BaseBlock`'s default cube was, for the ray trace and for the outline -- and
the comment above `GetBlockSelectionBox` in CraftWorld2.cpp had described the plate as though it were in
place, which is why the bug sat there: there was nothing in the tree that looked wrong. A cube is a legal
answer for every block, so a shape that is missing is invisible to a reader.

The symptom is what a cube costs. A cube in the cell directly in front of the player's feet intercepts the
look across a wide cone of angles -- everything steeper than about 22 degrees down here -- and the cell
the player is actually aiming at, one further along the line, is never the one reported. Since the
placement site then snaps a wire *target* up one cell (there is no sensible face to build against on a
floor plate), the click lands on the cell above the wire the player is looking past, which is not solid,
so dust cannot go there and the placement is silently refused. A line can only be extended by taking a
step forward between placements.

The plate is only the first half of the fix, and the second half is the one that is easy to miss because
the plate makes it *worse*. Every pick in the port used to march the look in fixed 0.075 steps and ask
whether the stepped point had landed inside the box. That has two holes for a thin shape, and both are
the stride's fault rather than the test's:

  * a point-in-box test cannot see a box the look crosses between two samples. Wire is 0.0625 tall, so
    any look steeper than `asin(0.0625 / 0.075)`, about 56 degrees, crosses it in less than one step.
  * the cell itself can fall between two samples. Stepping the sample along and clipping against the box
    of the cell it lands in fixes the first hole, but not this one: where the look clips the corner
    between a plate and the cell below it, the whole crossing of the cell can be shorter than the stride,
    so no sample ever lands in the cell whose plate the look crossed. Halving the stride helps and does
    not finish the job, because the crossing can be arbitrarily short at the corner.

So the loops no longer take a fixed stride. They step from cell wall to cell wall
(`CraftWorld::NextCellDistance`), one iteration per cell the ray actually crosses, and clip the ray
against that cell's box (`Ray::hasIntersected(box, distance)`, the call the placement face normal already
used). Neither half is optional: the walk is what makes every cell visitable, and the clip is what makes
a box inside a visited cell count wherever the sample itself happens to be.

Checked, all read out of the source rather than restated here:
  1. the plate is the full cell footprint in X and Z -- Java's bounds are 1 wide, not shrunk into a cross
     -- and is thinner than a cube;
  2. its top is exactly the height the mesher draws the wire strips at. That height is parsed out of
     `emitWireStrip` in CraftWorld_MeshBlocks.cpp, converted with the `pixel` constant it is written in,
     and required to be flat across the four corners of each strip. This is the coupling that matters:
     if the art moves and the crosshair does not, the crosshair stops matching what the player sees;
  3. the wire's *collision* shape is still its own 2 px floor plate and not the 1 px selection plate, so
     the aim fix was not smuggled into the field every walk, shove and mob test reads -- and it is still
     not a cube, which is what would make wire solid;
  4. every pick loop in both play states clips *and* walks cells, and no pick loop still asks "did the
     marched point land inside the box". This is a count as well as a shape: a new pick site is a new
     shape to check, and the stride has to be the cell walk or the corner hole comes back with it;
  5. the rule is *executed*, over a sweep of look pitch and eye height. The pick's answer is compared
     against the geometry computed independently -- the cell whose box the ray meets soonest, minimised
     over every cell of the test world -- and has to agree with it everywhere. The same sweep is run
     through the strides and shapes this replaces, and the counts say what each one costs: the cube that
     was there first, the 0.075 stride over the plate, and a half-plate 0.03125 stride over it, which is
     the "just step finer" fix that cannot reach the corner cases.

Run from anywhere:  python tools/check-wire-aim.py
"""
import math
import os
import re
import sys

GAME_ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
SCRIPTS = os.path.join(GAME_ROOT, "Scripts")
WORLD_CPP = os.path.join(SCRIPTS, "CraftWorld2.cpp")
WORLD_H = os.path.join(SCRIPTS, "CraftWorld2.h")
BLOCKS_CPP = os.path.join(SCRIPTS, "Blocks.cpp")
MESH_CPP = os.path.join(SCRIPTS, "CraftWorld_MeshBlocks.cpp")
STATES = [os.path.join(SCRIPTS, "StatePlay.cpp"), os.path.join(SCRIPTS, "StatePlayCreative.cpp")]

# How many pick loops each state has: the aim/outline that draws cubePos, the break/punch, and the
# placement march. StatePlayCreative has one more -- the creative punch has its own loop on top of the
# creative break -- so 3 + 4. A new pick site has to be written the same way and counted here.
PICK_SITES = {"StatePlay.cpp": 3, "StatePlayCreative.cpp": 4}

# Where the player is standing, and therefore the whole geometry of the test: on the floor, eye 1.62
# above it -- Java's player eye height -- with a wire in the cell directly in front at foot level and the
# floor continuing on past it. Looking down and forward at the *next* floor cell is the move being
# tested, and the yaw is straight down the +Z axis so the numbers are the ones a player faces head-on.
EYE_HEIGHT = 1.62
EYE = (0.5, EYE_HEIGHT, 0.5)
WIRE_CELL = (0, 0, 1)
NEXT_GROUND = (0, -1, 2)
UNDER_WIRE = (0, -1, 1)
CUBE = ((-0.5, -0.5, -0.5), (0.5, 0.5, 0.5))

# The strides this replaces, and the hair CraftWorld::NextCellDistance steps past each wall with.
OLD_STEP = 0.075
HALF_PLATE_STEP = 0.03125
HAIR = 0.0005
REACH = 5.25

failures = []


def fail(msg):
    failures.append(msg)
    print("  FAIL: %s" % msg)


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def vec3s(text):
    for m in re.finditer(r"Vector3\(\s*([^()]*?)\s*\)", text):
        parts = m.group(1).split(",")
        if len(parts) == 3:
            yield tuple(p.strip() for p in parts)


def as_float(s):
    s = s.strip().rstrip("fF")
    return float(s)


def cell_of(pt):
    return tuple(int(math.floor(v)) for v in pt)


def world_box(cell, box):
    """The AABB StatePlay builds from a cell and a centred local box."""
    return (tuple(cell[k] + 0.5 + box[0][k] for k in range(3)),
            tuple(cell[k] + 0.5 + box[1][k] for k in range(3)))


def clip(eye, d, box, cell):
    """Exact ray/box clip: the distance along the ray at which it enters, or None.

    A transcription of Ray::hasIntersected(BoundingBox, float&) from source/Aurora/Math/Ray.cpp: the
    slab test, with the entry clamped at 0 and a box wholly behind the eye reported as a miss.
    """
    lo, hi = world_box(cell, box)
    t0, t1 = 0.0, float("inf")
    for k in range(3):
        if abs(d[k]) < 1e-6:
            if eye[k] < lo[k] or eye[k] > hi[k]:
                return None
            continue
        inv = 1.0 / d[k]
        a = (lo[k] - eye[k]) * inv
        b = (hi[k] - eye[k]) * inv
        if a > b:
            a, b = b, a
        if a > t0:
            t0 = a
        if b < t1:
            t1 = b
        if t0 > t1:
            return None
    return t0


def next_cell_distance(eye, d, t):
    """CraftWorld::NextCellDistance, read off the C++ and walked the same way."""
    o, wall, past, live = [0.0] * 3, [0.0] * 3, [0.0] * 3, [False] * 3
    has_wall = [False] * 3
    first, first_axis = None, None
    for k in range(3):
        o[k] = eye[k]
        live[k] = abs(d[k]) >= 1e-6
        if not live[k]:
            continue
        # The cell at t, not at t + HAIR: see the comment in the port. Reading the probe's own cell as
        # the wall to head for steps over a cell entered inside the hair.
        cell = math.floor(eye[k] + d[k] * t)
        wall[k] = cell + (1.0 if d[k] > 0 else 0.0)
        at = (wall[k] - eye[k]) / d[k]
        if at > t:
            past[k] = wall[k] + (1.0 if d[k] > 0 else -1.0)
            has_wall[k] = True
            if first is None or at < first:
                first, first_axis = at, k
    if first is None:
        return t + 1.0

    # The distance at which the look leaves the cell this step enters: past the wall it crosses.
    second = None
    for k in range(3):
        if not has_wall[k]:
            continue
        w = past[k] if k == first_axis else wall[k]
        at = (w - o[k]) / d[k]
        if at > first and (second is None or at < second):
            second = at
    if second is not None and second < first + HAIR:
        # The cell between those two walls is crossed in less than the hair, so the sample goes in its
        # middle -- the same rule as the port, for a look down the shared edge of two blocks.
        return 0.5 * (first + second)
    return first + HAIR


def direction(pitch_deg):
    p = math.radians(pitch_deg)
    return (0.0, -math.sin(p), math.cos(p))


def truth(eye, pitch_deg, world, reach=5.25):
    """The geometry, computed with no reference to how the port walks the ray: the cell whose box the
    ray meets soonest, minimised over every cell of the test world."""
    d = direction(pitch_deg)
    best_cell, best_t = None, None
    for cell, box in world.items():
        t = clip(eye, d, box, cell)
        if t is None or t > reach:
            continue
        if best_t is None or t < best_t:
            best_cell, best_t = cell, t
    return best_cell


def hit_distance(eye, pitch_deg, world, cell):
    """The distance at which the look meets that cell's box, or None -- what the port calls the clip."""
    if cell is None:
        return None
    box = world.get(cell)
    if box is None:
        return None
    return clip(eye, direction(pitch_deg), box, cell)


def pick_walk(eye, pitch_deg, world, reach=5.25):
    """What the port now does: one step per cell wall, each cell's box clipped."""
    d = direction(pitch_deg)
    t = 0.0
    while t < reach:
        pt = tuple(eye[k] + d[k] * t for k in range(3))
        cell = cell_of(pt)
        box = world.get(cell)
        if box is not None and clip(eye, d, box, cell) is not None:
            return cell
        t = next_cell_distance(eye, d, t)
    return None


def pick_march(eye, pitch_deg, world, step, reach=5.25):
    """What the port did before: a fixed stride, accepting the first cell whose box *contains* the
    marched point."""
    d = direction(pitch_deg)
    i = 0.0
    while i < reach:
        pt = tuple(eye[k] + d[k] * i for k in range(3))
        cell = cell_of(pt)
        box = world.get(cell)
        if box is not None:
            wmin, wmax = world_box(cell, box)
            # The epsilon stands in for the float rounding in the same inclusive comparison.
            if all(wmin[k] <= pt[k] + 1e-9 and pt[k] <= wmax[k] + 1e-9 for k in range(3)):
                return cell
        i += step
    return None


def floor(cell):
    return {cell: CUBE}


def show(c):
    if c is None:
        return "-"
    if c == NEXT_GROUND:
        return "next ground"
    if c == UNDER_WIRE:
        return "under wire"
    if c == WIRE_CELL:
        return "the wire"
    return "(%d,%d,%d)" % c


def test_worlds(plate):
    """The four worlds every column of the scan is run against, as (label, world, of_interest)."""
    bare = dict(floor((0, -1, 0)))
    bare.update(floor(UNDER_WIRE))
    bare.update(floor(NEXT_GROUND))

    cube = dict(bare)
    cube[WIRE_CELL] = CUBE

    wire = dict(bare)
    wire[WIRE_CELL] = plate
    return bare, cube, wire


def main():
    src = read(WORLD_CPP)
    blocks = read(BLOCKS_CPP)
    mesh = read(MESH_CPP)

    # --- 1/2. The selection plate, read out of GetBlockSelectionBox. -------------------------------
    at = src.find("BoundingBox CraftWorld::GetBlockSelectionBox(")
    print("GetBlockSelectionBox")
    if at < 0:
        fail("CraftWorld::GetBlockSelectionBox is gone from CraftWorld2.cpp")
        return report()
    tail = src[at:at + 4000]

    if "Redstoneoff::getID()" not in tail:
        fail("GetBlockSelectionBox no longer singles out redstone wire. Every block answers with its "
             "collision shape, and the wire's collision shape is BaseBlock's default cube, which is the "
             "bug this check exists for -- see the docstring")
        return report()

    m = re.search(r"if\s*\(\s*b\s*==\s*Redstoneoff::getID\(\)\s*\)\s*\{\s*return\s+BoundingBox\((.*?)\)\s*;",
                  tail, re.S)
    if not m:
        fail("the wire branch in GetBlockSelectionBox does not return a BoundingBox this script can read")
        return report()
    corners = list(vec3s(m.group(1)))
    if len(corners) != 2:
        fail("the wire branch does not return a box of two corners")
        return report()
    lo = tuple(as_float(v) for v in corners[0])
    hi = tuple(as_float(v) for v in corners[1])
    print("  selection box: (%.4f, %.4f, %.4f) .. (%.4f, %.4f, %.4f)" % (lo + hi))

    for i, axis in ((0, "x"), (2, "z")):
        if lo[i] != -0.5 or hi[i] != 0.5:
            fail("the plate is not the full cell footprint on %s: %.4f .. %.4f. Java's wire bounds are "
                 "1 wide in both, so a narrowed box would leave the crosshair missing wire it should pick"
                 % (axis, lo[i], hi[i]))
    if hi[1] > 0.5 - 1e-6:
        fail("the plate is as tall as a cube (top %.4f); this is the full-cube answer that eats the aim"
             % hi[1])
    if hi[1] <= lo[1]:
        fail("the plate has no height: top %.4f is not above bottom %.4f" % (hi[1], lo[1]))

    # The height the mesher draws the strips at, in the pixel unit the mesher writes them in.
    if not re.search(r"pixel\s*=\s*\(float\)\s*1\s*/\s*16\.0f\s*;", mesh) and \
       not re.search(r"pixel\s*=\s*1\.0f\s*/\s*16\.0f\s*;", mesh):
        fail("`pixel` is not 1/16 in CraftWorld_MeshBlocks.cpp any more, so the plate heights this script "
             "compares are not comparable")
        return report()

    lam = re.search(r"emitWireStrip\s*=\s*\[&\][^\n]*\n\s*\{", mesh)
    if not lam:
        fail("the wire strip emitter (emitWireStrip) is gone from CraftWorld_MeshBlocks.cpp")
        return report()
    depth, i = 0, lam.end() - 1
    while i < len(mesh):
        if mesh[i] == "{":
            depth += 1
        elif mesh[i] == "}":
            depth -= 1
            if depth == 0:
                break
        i += 1
    strip = mesh[lam.end():i]
    rows = set()
    for c in vec3s(strip):
        # The strips are written as (x, y, z) in pixels, so the y component is the height.
        rows.add(as_float(c[1]))
    if len(rows) != 1:
        fail("the wire strips are drawn at %s different heights %s; the plate would not be flat"
             % (len(rows), sorted(rows)))
        return report()
    drawn_top = -0.5 + sorted(rows)[0] / 16.0
    print("  mesher draws the strips at %g px = %.4f above the cell floor" % (sorted(rows)[0], drawn_top - -0.5))
    print("  selection plate top:       %.4f" % hi[1])
    if abs(hi[1] - drawn_top) > 1e-6:
        fail("the selection plate tops out at %.4f but the mesher draws the wire at %.4f. Crosshair and "
             "art have to be the same surface or the player aims at a line they cannot see"
             % (hi[1], drawn_top))

    # --- 3. The collision shape is still the wire's own plate. --------------------------------------
    ctor = re.search(r"Redstoneoff::Redstoneoff\([^)]*\)[^{]*\{", blocks)
    if not ctor:
        fail("the Redstoneoff constructor could not be found in Blocks.cpp")
    else:
        end = blocks.find("\n}", ctor.end())
        body = blocks[ctor.end():end]
        cm = re.search(r"collideBox\s*=\s*BoundingBox\((.*?)\)\s*;", body, re.S)
        if not cm:
            fail("Redstoneoff no longer sets a collideBox of its own. That field is the *collision* "
                 "shape, which every walk, shove and mob test reads; the wire's own 2 px floor plate "
                 "belongs there and the 1 px selection plate belongs in GetBlockSelectionBox only")
        else:
            ccorners = list(vec3s(cm.group(1)))
            clo = tuple(as_float(v) for v in ccorners[0])
            chi = tuple(as_float(v) for v in ccorners[1])
            if abs(chi[1] - hi[1]) < 1e-6:
                fail("Redstoneoff's collideBox now tops out at %.4f, the selection plate's own height. "
                     "The plate belongs in GetBlockSelectionBox only: collideBox is what the walk, the "
                     "shove and the mob tests read, and wire is not solid" % chi[1])
            elif chi[1] > 0.5 - 1e-6:
                fail("Redstoneoff's collideBox is a full cube again (top %.4f), which is what made the "
                     "crosshair stick to a wire in front of the player's feet" % chi[1])
            else:
                print("  collision box is the wire's own plate (top %.4f), untouched by the pick shape"
                      % chi[1])

    # --- 4. Every pick loop walks cells and clips, and none still marches by containment. -----------
    print()
    print("pick loops")
    world_h = read(WORLD_H)
    if "NextCellDistance" not in world_h:
        fail("CraftWorld2.h does not declare NextCellDistance, so no pick loop can be walking cells")
    elif not re.search(r"float CraftWorld::NextCellDistance\(", src):
        fail("CraftWorld2.cpp does not define CraftWorld::NextCellDistance")

    for path in STATES:
        name = os.path.basename(path)
        text = read(path)
        if "testBox.contains(" in text:
            for ln, line in enumerate(text.splitlines(), 1):
                if "testBox.contains(" in line:
                    fail("%s:%d still accepts a block by testing whether the marched point is inside its "
                         "box. On a 1 px plate that test sees nothing at all -- see the docstring"
                         % (name, ln))
        clipped = len(re.findall(r"\w+Ray\.hasIntersected\(testBox,", text))
        walked = len(re.findall(r"CraftWorld::NextCellDistance\(", text))
        conditions = len(re.findall(r"BlockEditable\([^)]*\)\s*&&\s*\w+Dist != -1\.0f", text))
        want = PICK_SITES[name]
        for what, got in (("clipped ray/box tests", clipped), ("cell-walk strides", walked),
                          ("pick conditions", conditions)):
            if got != want:
                fail("%s: %d %s, expected %d. Every pick site has to walk cells *and* clip, and a new one "
                     "has to be counted in PICK_SITES" % (name, got, what, want))
                break
        else:
            print("  %-24s %d of %d pick loops walk cells and clip" % (name, clipped, want))

    # --- 5. Execute it: truth, the port's pick, and the strides it replaces. ------------------------
    plate = (lo, hi)
    bare, cube_world, wire_world = test_worlds(plate)

    print()
    print("  look-pitch scan, 5 deg to 80 deg (pitch is degrees below the horizon):")
    print("    %-9s %-14s %-14s %-14s %-14s %s"
          % ("pitch", "aimed at", "cube+march", "plate+march", "plate+walk", "placement"))
    rows_out = []
    for tenth in range(50, 801, 5):
        pitch = tenth / 10.0
        aimed = truth(EYE, pitch, bare)
        row = (pitch,
               aimed,
               pick_march(EYE, pitch, cube_world, OLD_STEP),
               pick_march(EYE, pitch, wire_world, OLD_STEP),
               pick_walk(EYE, pitch, wire_world))
        rows_out.append(row)
    # Print the band that matters -- where the player is aiming at the cell one along the line -- plus a
    # few degrees either side, so the seam is visible.
    for pitch, aimed, cube, marched, walked in rows_out:
        if not (25 <= pitch <= 55):
            continue
        placement = "-"
        if walked == NEXT_GROUND:
            placement = "next cell ok"
        elif walked is None:
            placement = "-"
        elif walked in (UNDER_WIRE,):
            placement = "into wire cell"
        elif walked == WIRE_CELL:
            placement = "at the wire"
        print("    %-9.1f %-14s %-14s %-14s %-14s %s"
              % (pitch, show(aimed), show(cube), show(marched), show(walked), placement))

    band = [r for r in rows_out if r[1] == NEXT_GROUND]
    if not band:
        fail("no look angle in the scan aims at the floor of the next cell; the geometry of this check no "
             "longer matches where the player stands")
        return report()
    cube_ok = [r for r in band if r[2] == NEXT_GROUND]
    marched_ok = [r for r in band if r[3] == NEXT_GROUND]
    walked_ok = [r for r in band if r[4] == NEXT_GROUND]
    print()
    print("  aimed at the next floor cell: %d look angles (%.1f deg to %.1f deg)"
          % (len(band), band[0][0], band[-1][0]))
    print("    cube + march:      %d of %d" % (len(cube_ok), len(band)))
    print("    plate + march:     %d of %d" % (len(marched_ok), len(band)))
    print("    plate + walk:      %d of %d" % (len(walked_ok), len(band)))

    if len(walked_ok) * 4 < len(band) * 3:
        fail("the walk only reaches the cell one along the line on %d of the %d look angles aimed at it. "
             "Aim has to survive the band, not just one angle in it" % (len(walked_ok), len(band)))
    if len(walked_ok) < len(marched_ok):
        fail("the walk reaches the cell on fewer angles than the old march (%d vs %d), so the pick is not "
             "doing what the loops now ask of it" % (len(walked_ok), len(marched_ok)))
    if len(walked_ok) <= len(cube_ok):
        fail("the plate reaches the cell no more often than the cube (%d vs %d), so either the plate is "
             "not in the source or the scan has stopped modelling a wire in the way -- this check would "
             "pass on the bug" % (len(walked_ok), len(cube_ok)))

    # --- 5b. The same comparison over pitch x eye height, against the geometry itself. --------------
    #
    # A player is not a fixed height above the floor: walking, jumping and the camera bob all move the
    # eye, and a fixed stride's answer depends on where its samples fall relative to a 0.0625 plate, which
    # is why the old symptom was intermittent and felt like a latch rather than a miss. The walk must not
    # depend on any of that: every look has to answer what the boxes say, and the strides are counted so
    # the table keeps saying what they cost.
    print()
    print("  sweep over pitch 30.0-89.9 deg at 41 eye heights, against the geometry of the boxes")
    counts = {"walk": 0, "march": 0, "half": 0, "cube": 0}
    looks = 0
    ties = 0
    walk_bad = []
    for hstep in range(-20, 21):
        eye = (EYE[0], EYE_HEIGHT + hstep * 0.0025, EYE[2])
        for tenth in range(300, 900):
            pitch = tenth / 10.0
            want = truth(eye, pitch, wire_world)
            want_hit = hit_distance(eye, pitch, wire_world, want)
            looks += 1
            got = pick_walk(eye, pitch, wire_world)
            got_hit = hit_distance(eye, pitch, wire_world, got)
            # Right means the pick meets the box at the distance the geometry does. A *different* cell at
            # the same distance is a look down the shared edge of two blocks, which both of them answer
            # (the clip enters both at that distance, the second with zero length); a hit further away, or
            # none where there is one, is the bug -- a cell crossed at a corner being stepped over.
            if want_hit is None:
                wrong = got_hit is not None and got_hit <= REACH
            else:
                wrong = got_hit is None or got_hit > want_hit + 1e-6
            if wrong:
                walk_bad.append((hstep * 0.0025, pitch, want, got))
            elif got != want:
                ties += 1
            if pick_march(eye, pitch, wire_world, OLD_STEP) != want:
                counts["march"] += 1
            if pick_march(eye, pitch, wire_world, HALF_PLATE_STEP) != want:
                counts["half"] += 1
            if pick_march(eye, pitch, cube_world, OLD_STEP) != want:
                counts["cube"] += 1
    counts["walk"] = len(walk_bad)
    print("    %d looks" % looks)
    print("    the pick the port runs (cell walk + clip) is wrong on %d" % counts["walk"])
    if ties:
        print("    of the rest, %d look down the shared edge of two blocks: the walk answers the other of "
              "the two,\n      at the same distance, which is the cell the clip enters with zero length"
              % ties)
    print("    the 0.075 stride over the plate:  %5d wrong  (%.2f%%)"
          % (counts["march"], 100.0 * counts["march"] / looks))
    print("    a 0.03125 stride over the plate:  %5d wrong  (%.2f%% -- not a smaller-step problem)"
          % (counts["half"], 100.0 * counts["half"] / looks))
    print("    the cube that was there first:    %5d wrong  (%.2f%%)"
          % (counts["cube"], 100.0 * counts["cube"] / looks))
    if walk_bad:
        hstep, pitch, want, got = walk_bad[0]
        fail("the pick the port runs disagrees with the geometry at eye %+.4f, pitch %.1f deg: it picks "
             "%s where the boxes say %s. The walk and the clip are both supposed to be the geometry, so "
             "one of the two is not doing its half" % (hstep, pitch, show(got), show(want)))
    for name, key in (("0.075 stride", "march"), ("cube", "cube")):
        if counts[key] == 0:
            fail("the %s agrees with the geometry on every look in the sweep, so this check has stopped "
                 "modelling the bug it exists for -- the walk would look like a no-op" % name)

    return report()


def report():
    print()
    if failures:
        print("%d problem(s)." % len(failures))
        return 1
    print("PASS: the crosshair sees over a wire in front of the player and lands on one when aimed at it,")
    print("      at the height the art draws it, at every look angle and eye height.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
