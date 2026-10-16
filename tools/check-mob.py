#!/usr/bin/env python3
"""
Audit an implemented mob end to end, and fail loudly on the things that fail silently.

Every bug this catches was invisible at build time. The mob compiled, spawned, walked and rendered;
it was just wrong in a way nothing reported:

  * geometry read from a third-party mirror instead of the jar, so it was unverifiable
  * a texture whose art does not actually cover the box the model samples
  * sounds never converted, with the call sites stubbed out to clear their own flags
  * alpha test / face culling set for an opaque skin, which blacks out or erases any mob that
    uses transparency
  * the mob's food missing from the right-click feed gate, so it follows you but cannot be fed
  * content the mob implies but nobody added -- a chicken needs feathers, raw/cooked chicken and
    eggs, and without them it is the only mob that drops nothing

Adding a mob is not finished when it walks. It is finished when this says so.

    python tools/check-mob.py chicken
    python tools/check-mob.py --all
    python tools/check-mob.py --list

Exits non-zero if any mob reports a failure, so it can gate a commit.
"""

import argparse
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

try:
    import jar_model_extract as jar
except ImportError:  # pragma: no cover - only when run from outside tools/
    jar = None

MESHES = os.path.join(ROOT, "Scripts", "CraftWorld_MobMeshes.cpp")
TEXTURES = os.path.join(ROOT, "Assets", "Textures")
SOUND_ROOTS = [ROOT, os.path.join(ROOT, "Minecraft - Compiled")]
SOUND_TIERS = ["Sounds", "SoundsLow"]
ITEMS_H = os.path.join(ROOT, "Scripts", "Items.h")
ITEMS_CPP = os.path.join(ROOT, "Scripts", "Items.cpp")
SAVE_LOAD = os.path.join(ROOT, "Scripts", "CraftWorld_SaveLoad.cpp")
TEXTURE_HELPER = os.path.join(ROOT, "Scripts", "TextureHelper.cpp")
MOBS_CPP = os.path.join(ROOT, "Scripts", "CraftWorld_Mobs.cpp")
LAME_MOB_CPP = os.path.join(ROOT, "Scripts", "LameMob.cpp")
CRAFT_WORLD_H = os.path.join(ROOT, "Scripts", "CraftWorld2.h")
PLAY_FILES = [os.path.join(ROOT, "Scripts", "StatePlay.cpp"),
              os.path.join(ROOT, "Scripts", "StatePlayCreative.cpp")]

# The pack the pixel-level checks read. Not the first alphabetically -- that would be a custom pack
# whose art may legitimately differ from vanilla.
DEFAULT_PACK = "Minecraft"
ITEM_ATLAS = os.path.join(TEXTURES, DEFAULT_PACK, "items.png")
ATLAS_CELL = 16

OK, WARN, FAIL = "OK", "WARN", "FAIL"

# What each mob is expected to own. `artifacts` is the part that keeps getting forgotten: the
# derived content a mob implies. Each entry is (item class, art name, kind), where the art name is
# what the item's cell in items.png should look like -- ordinary items have no PNG of their own.
MOBS = {
    "chicken": dict(
        cls="Chicken", cpp="Scripts/Chicken2.cpp",
        texture="entity/chicken/chicken.png",
        sounds=["hurt1", "hurt2", "say1", "say2", "say3", "step1", "step2", "plop"],
        breed="WheatSeeds",
        artifacts=[
            # (class, sprite, kind)  kind: loot = drops on death, derived = crafted/smelted, lay = produced while alive
            ("Feather", "feather", "loot"),
            ("RawChicken", "chicken", "loot"),
            ("CookedChicken", "cooked_chicken", "derived"),
            ("Egg", "egg", "lay"),
        ],
        # vanilla paints only the sole and one edge of each leg; the rest is meant to be see-through,
        # and the head box's underside is unpainted in vanilla because the neck covers it
        sparse_ok={"chickenLegVertices": "vanilla paints the sole plus one edge; the rest is intentionally clear",
                   "chickenHeadVertices": "vanilla leaves the head's underside clear; the neck covers it"},
    ),
    "pig": dict(
        cls="Pig", cpp="Scripts/Pig2.cpp",
        texture="entity/pig/pig.png",
        sounds=["death", "hurt1", "hurt2", "say1"],
        breed="Carrot",
        artifacts=[("RawPorkchop", "porkchop", "loot"),
                   ("CookedPorkchop", "cooked_porkchop", "derived")],
    ),
    "cow": dict(
        cls="Cow", cpp="Scripts/Cow2.cpp",
        texture="entity/cow/cow.png",
        sounds=["hurt1", "hurt2", "say1", "say2", "say3", "say4"],
        breed="Wheat",
        artifacts=[("RawBeef", "beef", "loot"),
                   ("Leather", "leather", "loot"),
                   ("Steak", "cooked_beef", "derived")],
    ),
    "sheep": dict(
        cls="Sheep", cpp="Scripts/Sheep2.cpp",
        texture="entity/sheep/sheep.png",
        sounds=["say1", "say2"],
        breed="Wheat",
        artifacts=[("RawMutton", "mutton", "loot"),
                   ("CookedMutton", "cooked_mutton", "derived")],
    ),
    "zombie": dict(
        cls="Zombie", cpp="Scripts/Zombie2.cpp",
        texture="entity/zombie/zombie.png",
        sounds=["hurt1", "hurt2", "say1", "say2", "say3", "death"],
        breed=None,          # hostile: not breedable
        artifacts=[("Bone", "bone", "loot"), ("Gunpowder", "gunpowder", "loot")],
        mesh_prefix=None,    # reuses the humanoid BODY/HEAD/HAT/HAND/LEG mesh, checked by hand
    ),
    "creeper": dict(
        cls="Creeper", cpp="Scripts/Creeper2.cpp",
        texture="entity/creeper/creeper.png",
        sounds=["hurt1", "hurt2"],
        breed=None,
        artifacts=[("Gunpowder", "gunpowder", "loot")],
        mesh_prefix=None,    # only creeperLegVertices exists; the rest is hand-built
    ),
    "spider": dict(
        cls="Spider", cpp="Scripts/Spider2.cpp",
        texture="entity/spider/spider.png",
        # Vanilla draws the whole model a second time with entity/spider_eyes.png to get the glow,
        # and that skin is clear except where the eyes are -- which is what forces the alpha test
        # and culling off on this mob, not the base skin.
        overlay_textures=["entity/spider/spider_eyes.png"],
        # 1.7.3 ships four idles, four footfalls and a death rattle, and no hurt clip of its own --
        # getHurtSound() points at mob.spider.say, so the hurt path reuses the idle files.
        sounds=["say1", "say2", "say3", "say4", "step1", "step2", "step3", "step4", "death"],
        breed=None,          # hostile: not breedable
        # Java EntitySpider.dropFewItems: 0-2 string always, then one spider eye at a 1-in-3 roll.
        artifacts=[("String", "string", "loot"), ("SpiderEye", "spider_eye", "loot")],
        # The eight legs are the same rotated ModelBox as the jar's, each floor-scraping box is
        # deliberately thin, and every leg samples the one 16x2x2 window, so the legs read as
        # "partly painted" against the whole-box coverage test by design.
        sparse_ok={"spiderLegVertices": "vanilla paints one 16x2x2 leg window and mirrors it eight times",
                   "spiderLegLeftVertices": "vanilla paints one 16x2x2 leg window and mirrors it eight times"},
    ),
    "skeleton": dict(
        cls="Skeleton", cpp="Scripts/Skeleton2.cpp",
        texture="entity/skeleton/skeleton.png",
        # 1.7.3 ships three idles, four hurts, four footfalls and a death rattle. Unlike the spider,
        # getHurtSound() is mob.skeleton.hurt, so the hurt clips are the mob's own.
        sounds=["say1", "say2", "say3", "hurt1", "hurt2", "hurt3", "hurt4",
                "step1", "step2", "step3", "step4", "death"],
        breed=None,          # hostile: not breedable
        # Java EntitySkeleton.dropFewItems: 0-2 bone, then 0-2 arrow, plus a bow on the rare roll.
        artifacts=[("Bone", "bone", "loot"), ("Arrow", "arrow", "loot"), ("Bow", "bow", "loot")],
        # ModelSkeleton extends the biped, so body and head are the zombie's shared buffers and only
        # the four thin limbs are its own; as with the zombie there is no single mesh to check, so the
        # model and coverage checks stay manual.
        mesh_prefix=None,
        # But the transparency check has to know what Render draws, and that is the shared biped body
        # and head plus the derived limbs -- see render_boxes(). This is the evidence behind the
        # cutout: the skeleton's own skin leaves the gaps between the ribs transparent and the body's
        # back face is largely unpainted, so with the alpha test off those texels paint black and with
        # culling on the far face is erased behind them.
        render_parts=["BODY", "HEAD", "skeletonHandVertices", "skeletonLegVertices"],
    ),
}


def read(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def packs():
    """The texture packs: directories under Assets/Textures that actually hold entity art.
    Filtering on the entity/ subtree keeps VCS bookkeeping like .svn out of the results."""
    return sorted(d for d in os.listdir(TEXTURES)
                  if os.path.isdir(os.path.join(TEXTURES, d, "entity")))


# ---------------------------------------------------------------------------------------------
# model: derive each part's box (W,H,D,u,v) from the emitted mesh code
#
# Not from the comments -- from the code. The comments are written by the same generator that
# writes the code, so trusting them would only confirm the generator agrees with itself. The mesh
# sets, per face, `left/right/down/up` as texture-window edges, and ModelBox's net is known, so the
# box can be read back out: u is the smallest window x, v the smallest window y, and the two
# distinct window widths are w and d. That holds whether or not the part is 90-degree rotated,
# because rotation only reassigns windows to faces.

FACE_BLOCK = re.compile(
    r"left = textureXpixel\*(-?[\d.]+);\s*"
    r"right = left \+ textureXpixel\*(-?[\d.]+);\s*"
    r"down = textureYpixel\*(-?[\d.]+);\s*"
    r"up = down \+ textureYpixel\*(-?[\d.]+);")
SECTION = re.compile(r"/// \* --- (\w+) --- \*")
ASSIGN = re.compile(r"^(left|right|down|up) = (.+);$")


def window_texels(body):
    """Texel-space (left, width, down, height) windows for one section.

    This file uses two assignment styles and both have to be read, or a whole rig goes unchecked:
    the generated sections write `left = textureXpixel*46;` and the hand-written biped sections (and
    `_tmp_atlas/skeleton_gen.py`, which deliberately matches their style) write literal normalised
    floats, `left = 0.71875f;`, sometimes as `right = left + 0.125f`.

    The two constants are the normalisation, and they are not the same number: the mesher sets
    `textureXpixel = 1/64.0f` and `textureYpixel = 1/32.0f` (a mob skin is 64 wide and 32 tall). So
    u divides by the texture width and v by its height -- converting both here means the box recovery
    below, and every coverage/transparency check built on it, is right by construction rather than by
    remembering which divisor a given section used.
    """
    env = {"textureXpixel": 1.0 / 64.0, "textureYpixel": 1.0 / 32.0}
    if re.search(r"textureYpixel\s*=\s*1/64", body):
        env["textureYpixel"] = 1.0 / 64.0
    faces = []
    window = None
    tex_pushes = 0
    for line in body.splitlines():
        line = line.strip()
        m = ASSIGN.match(line)
        if m:
            name, expr = m.groups()
            expr = re.sub(r"(?<=[\d.])f\b", "", expr)
            try:
                env[name] = eval(expr, {"__builtins__": {}}, env)
            except Exception:
                return []
            continue
        if line.startswith("mPosition.push_back"):
            if window is None:
                window = (env["left"], env["right"], env["down"], env["up"])
            continue
        if line.startswith("mtextures.push_back"):
            tex_pushes += 1
            if tex_pushes == 4:
                tex_pushes = 0
                # A face's window can be written either way round -- `up = down - textureYpixel*6` is
                # a real case (sheepHeadFurVertices), where the quad's v simply runs the other way --
                # so reduce each face to the rectangle it covers rather than trusting the sign.
                u0, u1 = sorted(window[0:2])
                v0, v1 = sorted(window[2:4])
                faces.append((int(round(u0 * 64.0)), int(round((u1 - u0) * 64.0)),
                              int(round(v0 * 32.0)), int(round((v1 - v0) * 32.0))))
                window = None
    return faces


def mesh_parts():
    """{part name: [(W,H,D,u,v), ...]} for every section whose boxes are recoverable."""
    src = read(MESHES)
    marks = [(m.start(), m.group(1)) for m in SECTION.finditer(src)]
    out = {}
    for i, (start, name) in enumerate(marks):
        end = marks[i + 1][0] if i + 1 < len(marks) else len(src)
        body = src[start:end]
        faces = window_texels(body)
        # A section can bake more than one box -- the chicken's head mesh carries head + bill +
        # chin -- and each box emits its six faces in order, so walk them in sixes.
        boxes = []
        for k in range(0, len(faces) - 5, 6):
            group = faces[k:k + 6]
            widths = sorted(f[1] for f in group)
            heights = sorted(f[3] for f in group)
            u = min(f[0] for f in group)
            v = min(f[2] for f in group)
            w = max(set(widths), key=widths.count)
            h = max(set(heights), key=heights.count)
            other_w = [x for x in widths if x != w]
            other_h = [x for x in heights if x != h]
            width_d = other_w[0] if other_w else w
            # d must agree whether it is read across the width or up the height -- a disagreement
            # means these six windows are not a ModelBox net at all
            if other_h and other_h[0] != width_d:
                continue
            boxes.append((w, h, width_d, u, v))
        if boxes:
            out[name] = boxes
    return out


def mob_boxes(parts, prefix):
    """[(part name, (W,H,D,u,v))] for the boxes a mob's mesh samples."""
    out = []
    for name, boxes in parts.items():
        if name.lower().startswith(prefix.lower()):
            for b in boxes:
                out.append((name, b))
    return out


def render_boxes(name, spec, parts):
    """[(part, box)] for the buffers a mob's Render actually draws.

    Defaults to the mesh_prefix slice. A mob that stitches more than one rig together cannot use a
    prefix at all: the skeleton draws the biped's shared BODY and HEAD plus its own derived
    skeletonHandVertices / skeletonLegVertices, and no single prefix covers those, so it names the
    sections explicitly. Without the list the transparency check has no evidence and reports the
    opposite of the truth -- "opaque skin but alpha test on" -- for a skin whose whole point is the
    gaps in the ribs.
    """
    names = spec.get("render_parts")
    if names:
        return [(n, b) for n in names for b in parts.get(n, [])]
    prefix = spec.get("mesh_prefix", name)
    return mob_boxes(parts, prefix) if prefix else []


def jar_boxes_by_class():
    """{obfuscated class: {(W,H,D,u,v)}} for every model class in the 1.7.3 client."""
    if jar is None:
        return {}, "tools/jar_model_extract.py not importable"
    jar_path = os.environ.get("MC173_JAR") or jar.DEFAULT_JAR
    if not os.path.isfile(jar_path):
        return {}, "no 1.7.3 client jar at %s" % jar_path
    try:
        javap = jar.find_javap()
    except SystemExit as exc:
        return {}, str(exc)
    out = {}
    for cls in jar.modelled_classes(jar_path):
        boxes = set()
        for b in jar.extract_class(javap, jar_path, cls):
            boxes.add((b["size"][0], b["size"][1], b["size"][2], b["uv"][0], b["uv"][1]))
        if boxes:
            out[cls] = boxes
    return out, None


# ---------------------------------------------------------------------------------------------
# checks. each returns (status, detail)

def mesh_sections():
    """Every section name in the mesh file, whether or not its boxes could be recovered."""
    return [m.group(1) for m in SECTION.finditer(read(MESHES))]


def check_model(name, spec, parts, jar_boxes, problems):
    prefix = spec.get("mesh_prefix", name)
    if prefix is None:
        return WARN, "no mesh of its own (built by hand); geometry not machine-checked"
    found = mob_boxes(parts, prefix)
    mine = set(b for _, b in found)
    # say so if a section exists but its faces could not be read back, rather than quietly
    # comparing a partial model against the jar and reporting a mismatch that is not real
    unparsed = [s for s in mesh_sections() if s.lower().startswith(prefix.lower()) and s not in parts]
    if not mine:
        if unparsed:
            return WARN, "sections %s exist but expose no recoverable ModelBox net" % ", ".join(unparsed)
        problems.append("no mesh sections found for prefix %r" % prefix)
        return FAIL, "no mesh sections match %r" % prefix
    if unparsed:
        return WARN, "read %d box(es) from %d part(s), but these do not expose a ModelBox net: %s" % (
            len(mine), len(found), ", ".join(unparsed))
    if not jar_boxes:
        return WARN, "derived %d box(es) but no jar to compare against" % len(mine)
    exact = [c for c, b in jar_boxes.items() if b == mine]
    if len(exact) == 1:
        return OK, "matches jar class %s exactly (%d distinct box(es) from %d part(s))" % (
            exact[0], len(mine), len(found))
    if len(exact) > 1:
        return OK, "matches jar classes %s (indistinguishable by box)" % ", ".join(sorted(exact))
    # closest candidate by shared boxes
    best, bestn = None, -1
    for c, b in jar_boxes.items():
        n = len(b & mine)
        if n > bestn:
            best, bestn = c, n
    missing = sorted(mine - jar_boxes[best]) if best else sorted(mine)
    problems.append("%s: %d mesh box(es) in no jar class, e.g. %s" % (name, len(missing), missing[:3]))
    return WARN, "no exact jar match; closest %s shares %d/%d box(es). Not in jar: %s" % (
        best, bestn, len(mine), missing[:3])


def check_texture(name, spec, parts, problems):
    rel = spec["texture"]
    have = packs()
    missing = [p for p in have if not os.path.isfile(os.path.join(TEXTURES, p, rel))]
    if len(missing) == len(have):
        problems.append("%s: texture %s is in no pack" % (name, rel))
        return FAIL, "%s not found in any of %d pack(s)" % (rel, len(have))
    found = [p for p in have if p not in missing]
    # read the default pack, so a custom pack's art is not mistaken for the shipped result
    probe = DEFAULT_PACK if DEFAULT_PACK in found else found[0]
    path = os.path.join(TEXTURES, probe, rel)
    try:
        from PIL import Image
    except ImportError:
        return WARN, "present in %d/%d pack(s); install Pillow for the coverage check" % (len(found), len(have))

    im = Image.open(path).convert("RGBA")
    px = im.load()
    W, H = im.size
    prefix = spec.get("mesh_prefix", name)
    found_boxes = mob_boxes(parts, prefix) if prefix else []
    if not found_boxes:
        return WARN, "texture %sx%s in %d/%d pack(s); no mesh boxes to check coverage against" % (
            W, H, len(found), len(have))

    sparse_ok = spec.get("sparse_ok", {})
    blank, low = [], []
    for part, (w, h, d, u, v) in found_boxes:
        # ModelBox net for this box, in texels
        cells = []
        for (rx, ry, rw, rh) in [(u + d, v, w, d), (u + d + w, v, w, d), (u, v + d, d, h),
                                 (u + d, v + d, w, h), (u + d + w, v + d, d, h), (u + 2 * d + w, v + d, w, h)]:
            for y in range(ry, ry + rh):
                for x in range(rx, rx + rw):
                    cells.append((x, y) if 0 <= x < W and 0 <= y < H else None)
        inside = [c for c in cells if c]
        if len(inside) != len(cells):
            problems.append("%s: box %dx%dx%d @%d,%d falls off the %dx%d texture" % (name, w, h, d, u, v, W, H))
            blank.append((part, (w, h, d, u, v)))
            continue
        painted = sum(1 for (x, y) in inside if px[x, y][3] > 0)
        if painted == 0:
            blank.append((part, (w, h, d, u, v)))
        elif painted < len(inside):
            low.append((part, (w, h, d, u, v), painted, len(inside)))

    label = "%sx%s in %d/%d pack(s)" % (W, H, len(found), len(have))

    # a fully clear box is only acceptable when the mob declares why, by part name
    unexpected = [b for b in blank if b[0] not in sparse_ok]
    if unexpected:
        names = ["%s %dx%dx%d@%d,%d" % ((p,) + b) for p, b in unexpected]
        problems.append("%s: %d box(es) sample fully transparent texture: %s" % (name, len(unexpected), names))
        return FAIL, "%s; %d box(es) entirely transparent: %s" % (label, len(unexpected), names)
    if blank:
        why = sparse_ok.get(blank[0][0], "")
        return OK, "%s; %d box(es) intentionally clear (%s)" % (label, len(blank), why or "declared")

    def fmt(entries):
        return "; ".join("%s %dx%dx%d@%d,%d %d/%d px" % (p, b[0], b[1], b[2], b[3], b[4], n, t)
                         for p, b, n, t in entries)

    low_unexpected = [e for e in low if e[0] not in sparse_ok]
    if low_unexpected:
        problems.append("%s: box(es) only partly painted: %s" % (name, fmt(low_unexpected)))
        return WARN, "%s; partly-painted box(es): %s" % (label, fmt(low_unexpected))
    if low:
        return OK, "%s; sparse as expected: %s" % (label, fmt(low))
    return OK, "%s; every sampled box fully painted" % label


def check_sounds(name, spec, problems):
    want = spec.get("sounds") or []
    if not want:
        return OK, "no clips expected"
    bad = []
    for root in SOUND_ROOTS:
        if name != "chicken" and not os.path.isdir(os.path.join(root, "Assets", "Sounds", "mobs", name)):
            continue
        for tier in SOUND_TIERS:
            d = os.path.join(root, "Assets", tier, "mobs", name)
            for clip in want:
                if not os.path.isfile(os.path.join(d, clip + ".wav")):
                    bad.append("%s/%s/%s.wav" % (os.path.relpath(root, ROOT) if root != ROOT else ".", tier, clip))
    if bad:
        problems.append("%s: %d missing clip file(s), first: %s" % (name, len(bad), bad[0]))
        return FAIL, "%d missing file(s), e.g. %s" % (len(bad), ", ".join(bad[:3]))
    return OK, "%d clip(s) present in both tiers" % len(want)


def check_breed(name, spec, problems):
    breed = spec.get("breed")
    if breed is None:
        return OK, "not breedable (by design)"
    src = read(os.path.join(ROOT, spec["cpp"]))
    m = re.search(r"breedItemId\s*=\s*(\w+)::getID\(\)", src)
    if not m:
        problems.append("%s: no breedItemId assignment in %s" % (name, spec["cpp"]))
        return FAIL, "no breedItemId assignment"
    if m.group(1) != breed:
        problems.append("%s: breedItemId is %s, expected %s" % (name, m.group(1), breed))
        return WARN, "breedItemId is %s (table says %s)" % (m.group(1), breed)

    # the right-click feed path is gated on a literal list; if the mob's food is not on it the mob
    # follows you but can never be fed, which reads as a physics bug rather than a missing item
    absent = []
    for path in PLAY_FILES:
        body = read(path)
        gate = [l for l in body.splitlines() if "feed: love mode" in l]
        if not gate:
            absent.append("%s (no feed gate found)" % os.path.basename(path))
        elif ("== %s::getID()" % breed) not in gate[0]:
            absent.append(os.path.basename(path))
    if absent:
        problems.append("%s: %s missing from the feed gate in %s" % (name, breed, ", ".join(absent)))
        return FAIL, "%s is NOT in the feed gate in %s" % (breed, ", ".join(absent))
    return OK, "%s set and reachable from both feed gates" % breed


def atlas_cell_painted(items_cpp, cls, problems, name):
    """Does the item's declared cell in items.png actually have art?

    Items declare where they live in the atlas with `textureRow` and `plane`; there is no per-item
    PNG for ordinary items (only special ones like the clock and the spawn eggs have their own file).
    So the presence of a sprite is: the constructor names a row and a plane, and that cell of the
    atlas is not empty. Returns (row, plane, painted, total) or None.
    """
    m = re.search(r"%s::%s\s*\(\s*\)\s*:\s*BaseItem\s*\(\s*\)\s*\{(.*?)\}" % (re.escape(cls), re.escape(cls)),
                  items_cpp, re.S)
    if not m:
        return None
    body = m.group(1)
    row = re.search(r"textureRow\s*=\s*(\d+)", body)
    plane = re.search(r"\bplane\s*=\s*(\d+)", body)
    if not row or not plane:
        return None
    row, plane = int(row.group(1)), int(plane.group(1))
    try:
        from PIL import Image
    except ImportError:
        return (row, plane, None, None)
    if not os.path.isfile(ITEM_ATLAS):
        return (row, plane, None, None)
    im = Image.open(ITEM_ATLAS).convert("RGBA")
    px = im.load()
    W, H = im.size
    cw = W // ATLAS_CELL
    # textureRow counts up from the BOTTOM of the atlas, not the top: CraftWorld::GetItemVerts
    # computes startPixelsY = 256 - ((textureRow + 1) * 16). Reading it top-down reports empty cells
    # that are not empty.
    x0, y0 = plane * cw, (H // ATLAS_CELL - 1 - row) * cw
    painted = total = 0
    for y in range(y0, y0 + cw):
        for x in range(x0, x0 + cw):
            if 0 <= x < W and 0 <= y < H:
                total += 1
                if px[x, y][3] > 0:
                    painted += 1
    return (row, plane, painted, total)


def check_artifacts(name, spec, problems):
    arts = spec.get("artifacts") or []
    if not arts:
        return OK, "no derived content expected"
    items_h = read(ITEMS_H)
    items_cpp = read(ITEMS_CPP)
    creative = read(PLAY_FILES[1])
    play = read(PLAY_FILES[0])

    details, missing = [], []
    for cls, sprite, kind in arts:
        decl = re.search(r"class\s+%s\b" % re.escape(cls), items_h) is not None
        defd = re.search(r"\b%s::getID\s*\(" % re.escape(cls), items_cpp) is not None
        has_id = defd
        cell = atlas_cell_painted(items_cpp, cls, problems, name) if decl else None
        if cell is None:
            sprite_ok = None
            cell_txt = "no textureRow/plane"
        elif cell[2] is None:
            sprite_ok = None
            cell_txt = "atlas cell row=%d plane=%d (not checked)" % (cell[0], cell[1])
        else:
            sprite_ok = cell[2] > 0
            cell_txt = "atlas r%d p%d %d/%d px" % (cell[0], cell[1], cell[2], cell[3])
        in_creative = cls in creative
        furn = ""
        if kind == "derived":
            fm = re.search(r"furnItem\s*=\s*(\d+)", items_cpp)
            furn = " (furnItem set)" if fm else " (NO furnItem)"
        details.append("%s[%s] class=%s id=%s %s creative=%s%s" % (
            cls, kind, "yes" if decl else "NO", "yes" if has_id else "NO",
            cell_txt, "yes" if in_creative else "NO", furn))
        if not (decl and has_id) or sprite_ok is False:
            missing.append(cls)

    # does the mob actually drop anything when destroyed?
    drops = False
    marker = "Test%s->toDestroy" % spec["cls"]
    idx = play.find(marker)
    if idx != -1:
        block = play[idx:idx + 1500]
        cut = block.find("Destroy%s(" % spec["cls"])
        if cut != -1:
            block = block[:cut]
        drops = "DropThis" in block

    lay = [c for c, s, k in arts if k == "lay"]
    if missing:
        problems.append("%s: %d artifact item(s) incomplete: %s" % (name, len(missing), ", ".join(missing)))
        return FAIL, "%d item(s) incomplete: %s | drops=%s" % (
            len(missing), ", ".join(missing), "yes" if drops else "NO")
    if not drops:
        problems.append("%s: destroy block has no DropThis call" % name)
        return FAIL, "items exist but the mob drops nothing on death"
    return OK, "%d artifact(s) present; drops=%s%s" % (
        len(arts), "yes", "" if not lay else ", lays %s" % ", ".join(lay))


def check_render(name, spec, parts, problems):
    src = read(os.path.join(ROOT, spec["cpp"]))
    if not src:
        problems.append("%s: %s not readable" % (name, spec["cpp"]))
        return FAIL, "%s not readable" % spec["cpp"]

    # Only the texels the model actually samples matter. A skin can hold transparent regions nothing
    # maps to (the pig's atlas padding, say) and drawing those is harmless.
    sampled_blank = 0
    overlay_blank = 0
    probe = os.path.join(TEXTURES, DEFAULT_PACK, spec["texture"])
    overlays = spec.get("overlay_textures") or []
    boxes = render_boxes(name, spec, parts)
    if boxes and os.path.isfile(probe):
        try:
            from PIL import Image
            im = Image.open(probe).convert("RGBA")
            px = im.load()
            W, H = im.size
            for _, (w, h, d, u, v) in boxes:
                for (rx, ry, rw, rh) in [(u + d, v, w, d), (u + d + w, v, w, d), (u, v + d, d, h),
                                         (u + d, v + d, w, h), (u + d + w, v + d, d, h),
                                         (u + 2 * d + w, v + d, w, h)]:
                    for y in range(ry, ry + rh):
                        for x in range(rx, rx + rw):
                            if 0 <= x < W and 0 <= y < H and px[x, y][3] == 0:
                                sampled_blank += 1
            # A second skin drawn over the same boxes (the spider's eyes) is transparent except
            # where the glow is, so it needs the alpha test whatever the base skin looks like.
            for rel in overlays:
                path = os.path.join(TEXTURES, DEFAULT_PACK, rel)
                if not os.path.isfile(path):
                    continue
                oim = Image.open(path).convert("RGBA")
                if any(p[3] == 0 for p in oim.get_flattened_data()):
                    overlay_blank += 1
        except ImportError:
            return WARN, "install Pillow to check whether the skin needs alpha test"
    needs_cutout = (sampled_blank > 0) or (overlay_blank > 0)

    alpha = "sceGuEnable(GU_ALPHA_TEST)" in src
    nocull = "sceGuDisable(GU_CULL_FACE)" in src
    if needs_cutout and not (alpha and nocull):
        # a real defect for a new mob, but the older mobs predate the convention and read fine,
        # so this is reported for a human to judge rather than treated as a hard break
        why = ("the overlay skin is transparent" if overlay_blank
               else "%d sampled texel(s) are transparent" % sampled_blank)
        problems.append("%s: %s but %s" % (
            name, why,
            "alpha test is off (they draw as black)" if not alpha else "face culling is on (they erase the far face)"))
        return WARN, "%s; alpha_test=%s cull_off=%s" % (why, alpha, nocull)
    if not needs_cutout and alpha and nocull:
        return WARN, "opaque skin but alpha test on and culling off (harmless, unnecessary)"
    transparent = []
    if sampled_blank:
        transparent.append("%d sampled texel(s)" % sampled_blank)
    if overlay_blank:
        transparent.append("%d transparent overlay(s)" % overlay_blank)
    return OK, "alpha_test=%s cull_off=%s (%s)" % (
        alpha, nocull, ", ".join(transparent) if transparent else "no transparency")


def check_wiring(name, spec, problems):
    cls = spec["cls"]
    world_h = read(CRAFT_WORLD_H)
    mobs = read(MOBS_CPP)
    save = read(SAVE_LOAD)
    helper = read(TEXTURE_HELPER)
    absent = []
    if not re.search(r"std::vector<%s\s*\*>\s*m%s" % (cls, cls + "s"), world_h):
        absent.append("m%ss vector in CraftWorld2.h" % cls)
    if "::Spawn%s(" % cls not in mobs:
        absent.append("CraftWorld::Spawn%s" % cls)
    if "::Destroy%s(" % cls not in mobs:
        absent.append("CraftWorld::Destroy%s" % cls)
    if cls not in save:
        absent.append("save/load blob")
    # TextureHelper names its entries <mob>Texture rather than after the class
    if ("%sTexture" % name) not in helper and spec["texture"] not in helper:
        absent.append("TextureHelper entry")
    # The crosshair ray. LameMob.cpp walks the mob lists by hand -- there is no registry -- and that
    # walk is the only thing that lets the player punch, ignite or click the mob. Nothing in the mob's
    # own files hints that it exists, so it is exactly the line that gets forgotten: the skeleton
    # shipped updating, rendering, shooting and burning, and could not be hit at all. Scoped between
    # the two walk functions, because mFoo also appears in FindMobForUse and a looser slice would pass
    # on the use walk alone.
    lame = read(LAME_MOB_CPP)
    ray_body = lame.split("LameMob* FindMobAlongRay")[-1].split("LameMob* FindMobForUse")[0]
    if ray_body.count("m%ss" % cls) == 0:
        absent.append("m%ss absent from LameMob::FindMobAlongRay" % cls)
    if absent:
        problems.append("%s: wiring missing: %s" % (name, ", ".join(absent)))
        return FAIL, ", ".join(absent)
    return OK, "vector, spawn/destroy, save/load, texture helper and crosshair ray all present"


def check_spawn_egg(name, spec, problems):
    cls = spec["cls"]
    items_h = read(ITEMS_H)
    mobs = read(MOBS_CPP)
    creative = read(PLAY_FILES[1])
    egg = "%sSpawnEgg" % cls
    if not re.search(r"class\s+%s\b" % egg, items_h):
        return WARN, "no %s class" % egg
    absent = []
    if ("%s::getID()" % egg) not in mobs:
        absent.append("IsSpawnEgg entry")
    if egg not in creative:
        absent.append("creative menu entry")
    if absent:
        problems.append("%s: spawn egg not wired for %s" % (name, ", ".join(absent)))
        return FAIL, ", ".join(absent)
    return OK, "%s present and wired" % egg


CHECKS = [
    ("model", lambda n, s, p, j, pr: check_model(n, s, p, j, pr)),
    ("texture", lambda n, s, p, j, pr: check_texture(n, s, p, pr)),
    ("sounds", lambda n, s, p, j, pr: check_sounds(n, s, pr)),
    ("breed", lambda n, s, p, j, pr: check_breed(n, s, pr)),
    ("artifacts", lambda n, s, p, j, pr: check_artifacts(n, s, pr)),
    ("render", lambda n, s, p, j, pr: check_render(n, s, p, pr)),
    ("wiring", lambda n, s, p, j, pr: check_wiring(n, s, pr)),
    ("egg", lambda n, s, p, j, pr: check_spawn_egg(n, s, pr)),
]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mobs", nargs="*", help="mobs to audit")
    ap.add_argument("--all", action="store_true", help="audit every mob in the table")
    ap.add_argument("--list", action="store_true", help="list the mobs and what is expected of each")
    args = ap.parse_args()

    if args.list:
        for name, spec in sorted(MOBS.items()):
            print("%s:" % name)
            print("   texture   %s" % spec["texture"])
            print("   sounds    %s" % (", ".join(spec.get("sounds") or []) or "-"))
            print("   breed     %s" % (spec.get("breed") or "not breedable"))
            for c, s, k in spec.get("artifacts") or []:
                print("   %-9s %-16s (art: %s)" % (k, c, s))
        return 0

    wanted = sorted(MOBS) if args.all else args.mobs
    if not wanted:
        print("nothing to do: name a mob, or pass --all / --list")
        return 1
    unknown = [m for m in wanted if m not in MOBS]
    if unknown:
        raise SystemExit("unknown mob(s): %s (known: %s)" % (", ".join(unknown), ", ".join(sorted(MOBS))))

    parts = mesh_parts()
    jar_boxes, jar_err = jar_boxes_by_class()
    if jar_err:
        print("note: cannot compare geometry against the jar -- %s\n" % jar_err)

    total_fail = 0
    for name in wanted:
        spec = MOBS[name]
        print("=== %s ===" % name)
        problems = []
        for label, fn in CHECKS:
            status, detail = fn(name, spec, parts, jar_boxes, problems)
            if status == FAIL:
                total_fail += 1
            print("  %-10s %-4s %s" % (label, status, detail))
        if problems:
            print("  -> %d issue(s):" % len(problems))
            for p in problems:
                print("       %s" % p)
        print()

    if total_fail:
        print("%d check(s) failed." % total_fail)
        return 1
    print("all checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
