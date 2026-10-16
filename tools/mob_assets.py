"""
Vanilla mob asset importer -- pulls a mob's exact texture and exact model geometry out of the
real Minecraft Java 1.7.3 install and parks them under tools/vanilla-assets/, ready to ship.

Why this exists: earlier the chicken's art and geometry were hand-authored against a guessed UV
layout, which produced a bird that did not match its texture and a texture that was invented rather
than extracted. This tool removes the guesswork. Everything it writes is copied or parsed from
vanilla 1.7.3 itself, so a mob's assets are on hand the moment implementation starts.

Two sources, because 1.7.3 splits them:

  textures  the client jar stores assets/ UNOBFUSCATED, so these are copied out verbatim, byte for
            byte, for any mob. This is fully automatic and needs nothing but the jar.
  geometry  the models are Java classes (net.minecraft.client.model.ModelX) and 1.7.3 ships them
            OBFUSCATED -- the jar has no ModelChicken, just classes named like `azd`. The compiler
            baked the box numbers into bytecode, so they are not readable by name. Geometry is
            therefore parsed from decompiled sources kept in tools/vanilla-models/, using the same
            parser the mesh generator uses (tools/mcjava_to_json.py). Drop a ModelX.java in there
            and this tool picks it up.

`fetch` writes, per mob:

    tools/vanilla-assets/<mob>/texture/<name>.png     the authentic vanilla texture(s)
    tools/vanilla-assets/<mob>/model.json             bones, cubes, pivots, derived mesh extents
    tools/vanilla-assets/MANIFEST.json                everything above, machine-readable
    tools/vanilla-assets/MANIFEST.md                  the same as a readiness table

and `fetch --install <pack-dir>` also copies each texture to its engine path under a texture pack.

The model.json numbers are DERIVED, never transcribed: cube extents come from replaying vanilla's
pivots through the engine's axis convention, exactly as tools/check-chicken-vanilla.py verifies for
the chicken. Mesh extents are reported in vanilla model pixels (1 px = 1/16 block).

Honest limits, so nothing here gets mistaken for more than it is:
  * no vanilla hitbox is published. A mob's hitbox really comes from EntityX.setSize(w, h) in the
    Entity sources, which are not in the jar. `modelBounds` below is what the *model* measures, and
    is labelled as such -- do not pass it off as vanilla's entity size.
  * models without a decompiled source are reported as missing, not guessed.

Usage:
    python tools/mob_assets.py                      # readiness table
    python tools/mob_assets.py textures             # inventory the jar's entity textures
    python tools/mob_assets.py fetch chicken        # one mob
    python tools/mob_assets.py fetch --all
    python tools/mob_assets.py fetch --all --install "Assets/Textures/Minecraft"
    python tools/mob_assets.py fetch --texture entity/spider_eyes.png   # loose extra texture

Options:
    --jar PATH   1.7.3 client jar (default: MC173_JAR env var, else the usual launcher path)
    --out DIR    output root (default tools/vanilla-assets)
    --force      re-write files that already exist
"""
import argparse
import hashlib
import json
import os
import shutil
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import mcjava_to_json as MJ          # the shared Model*.java parser, so geometry cannot drift

ASSETS = os.path.join(HERE, "vanilla-assets")
MODELS = os.path.join(HERE, "vanilla-models")
JAR_IN_JAR = "assets/minecraft/textures/"

# Where the launcher keeps the genuine client. Overridable with --jar or MC173_JAR.
DEFAULT_JAR = os.path.join(
    os.environ.get("APPDATA", os.path.expanduser("~")),
    ".minecraft", "versions", "1.7.3", "1.7.3.jar")

# ---------------------------------------------------------------------------------------------
# The registry. `textures` maps a path inside the jar to the path the engine loads it from -- the
# engine folders them by mob (entity/cow/cow.png) where the jar often does not (entity/chicken.png).
# `model` lists the decompiled classes to look for, in priority order; a mob whose sources are not
# present yet shows up as "model -" in the table rather than silently pretending to be ready.

MOBS = {
    "chicken":        {"model": ["ModelChicken"],
                       "textures": {"entity/chicken.png": "entity/chicken/chicken.png"}},
    "pig":            {"model": ["ModelPig"],
                       "textures": {"entity/pig/pig.png": "entity/pig/pig.png",
                                    "entity/pig/pig_saddle.png": "entity/pig/pig_saddle.png"}},
    "cow":            {"model": ["ModelCow"],
                       "textures": {"entity/cow/cow.png": "entity/cow/cow.png",
                                    "entity/cow/mooshroom.png": "entity/cow/mooshroom.png"}},
    "sheep":          {"model": ["ModelSheep1", "ModelSheep2"],
                       "textures": {"entity/sheep/sheep.png": "entity/sheep/sheep.png",
                                    "entity/sheep/sheep_fur.png": "entity/sheep/sheep_fur.png"}},
    "creeper":        {"model": ["ModelCreeper"],
                       "textures": {"entity/creeper/creeper.png": "entity/creeper/creeper.png",
                                    "entity/creeper/creeper_armor.png": "entity/creeper/creeper_armor.png"}},
    "zombie":         {"model": ["ModelZombie"],
                       "textures": {"entity/zombie/zombie.png": "entity/zombie/zombie.png"}},
    "zombie_villager": {"model": ["ModelZombieVillager"],
                       "textures": {"entity/zombie/zombie_villager.png": "entity/zombie/zombie_villager.png"}},
    "skeleton":       {"model": ["ModelSkeleton"],
                       "textures": {"entity/skeleton/skeleton.png": "entity/skeleton/skeleton.png",
                                    "entity/skeleton/wither_skeleton.png": "entity/skeleton/wither_skeleton.png"}},
    "spider":         {"model": ["ModelSpider"],
                       "textures": {"entity/spider/spider.png": "entity/spider/spider.png",
                                    "entity/spider/cave_spider.png": "entity/spider/cave_spider.png",
                                    "entity/spider_eyes.png": "entity/spider_eyes.png"}},
    "enderman":       {"model": ["ModelEnderman"],
                       "textures": {"entity/enderman/enderman.png": "entity/enderman/enderman.png",
                                    "entity/enderman/enderman_eyes.png": "entity/enderman/enderman_eyes.png"}},
    "wolf":           {"model": ["ModelWolf"],
                       "textures": {"entity/wolf/wolf.png": "entity/wolf/wolf.png",
                                    "entity/wolf/wolf_angry.png": "entity/wolf/wolf_angry.png",
                                    "entity/wolf/wolf_tame.png": "entity/wolf/wolf_tame.png",
                                    "entity/wolf/wolf_collar.png": "entity/wolf/wolf_collar.png"}},
    "ocelot":         {"model": ["ModelOcelot"],
                       "textures": {"entity/cat/ocelot.png": "entity/cat/ocelot.png",
                                    "entity/cat/black.png": "entity/cat/black.png",
                                    "entity/cat/red.png": "entity/cat/red.png",
                                    "entity/cat/siamese.png": "entity/cat/siamese.png"}},
    "villager":       {"model": ["ModelVillager"],
                       "textures": {"entity/villager/villager.png": "entity/villager/villager.png",
                                    "entity/villager/farmer.png": "entity/villager/farmer.png",
                                    "entity/villager/butcher.png": "entity/villager/butcher.png",
                                    "entity/villager/librarian.png": "entity/villager/librarian.png",
                                    "entity/villager/priest.png": "entity/villager/priest.png",
                                    "entity/villager/smith.png": "entity/villager/smith.png"}},
    "iron_golem":     {"model": ["ModelIronGolem"],
                       "textures": {"entity/iron_golem.png": "entity/iron_golem/iron_golem.png"}},
    "snowman":        {"model": ["ModelSnowMan"],
                       "textures": {"entity/snowman.png": "entity/snowman/snowman.png"}},
    "blaze":          {"model": ["ModelBlaze"],
                       "textures": {"entity/blaze.png": "entity/blaze/blaze.png"}},
    "ghast":          {"model": ["ModelGhast"],
                       "textures": {"entity/ghast/ghast.png": "entity/ghast/ghast.png",
                                    "entity/ghast/ghast_shooting.png": "entity/ghast/ghast_shooting.png"}},
    "slime":          {"model": ["ModelSlime"],
                       "textures": {"entity/slime/slime.png": "entity/slime/slime.png"}},
    "magma_cube":     {"model": ["ModelMagmaCube"],
                       "textures": {"entity/slime/magmacube.png": "entity/slime/magmacube.png"}},
    "bat":            {"model": ["ModelBat"],
                       "textures": {"entity/bat.png": "entity/bat/bat.png"}},
    "squid":          {"model": ["ModelSquid"],
                       "textures": {"entity/squid.png": "entity/squid/squid.png"}},
    "wither":         {"model": ["ModelWither"],
                       "textures": {"entity/wither/wither.png": "entity/wither/wither.png",
                                    "entity/wither/wither_armor.png": "entity/wither/wither_armor.png",
                                    "entity/wither/wither_invulnerable.png": "entity/wither/wither_invulnerable.png"}},
    "enderdragon":    {"model": ["ModelDragon"],
                       "textures": {"entity/enderdragon/dragon.png": "entity/enderdragon/dragon.png"}},
    "silverfish":     {"model": ["ModelSilverfish"],
                       "textures": {"entity/silverfish.png": "entity/silverfish/silverfish.png"}},
    "witch":          {"model": ["ModelWitch"],
                       "textures": {"entity/witch.png": "entity/witch/witch.png"}},
    "zombie_pigman":  {"model": ["ModelZombie"],
                       "textures": {"entity/zombie_pigman.png": "entity/zombie_pigman/zombie_pigman.png"}},
    "player":         {"model": ["ModelBiped"],
                       "textures": {"entity/steve.png": "entity/steve/steve.png"}},
}


# ---------------------------------------------------------------------------------------------
# jar

def find_jar(explicit=None):
    cand = [explicit, os.environ.get("MC173_JAR"), DEFAULT_JAR]
    for path in cand:
        if path and os.path.isfile(path):
            return path
    raise SystemExit(
        "No 1.7.3 client jar found. Looked at:\n" +
        "\n".join("  " + (c or "(unset)") for c in cand) +
        "\nPass --jar PATH or set MC173_JAR.")


def open_jar(path):
    z = zipfile.ZipFile(path)
    if not any(n.startswith(JAR_IN_JAR) for n in z.namelist()):
        raise SystemExit("%s does not look like a Minecraft client jar (no %s entries)"
                         % (path, JAR_IN_JAR))
    return z


def jar_entity_textures(z):
    out = []
    for n in z.namelist():
        if n.startswith(JAR_IN_JAR + "entity/") and n.lower().endswith(".png"):
            out.append(n[len(JAR_IN_JAR):])
    return sorted(out)


def entity_names(z):
    """The set of `entity/...` paths this jar actually contains."""
    return set(jar_entity_textures(z))


def read_texture(z, jar_path):
    """Read a texture by its `entity/...` path, or None if the jar has no such entry."""
    full = JAR_IN_JAR + jar_path
    if full not in z.namelist():
        return None
    return z.read(full)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------------------------------------
# model geometry

def parse_model(mob, spec):
    """Parse every decompiled model source we have for this mob. Returns (docs, missing)."""
    docs, missing = [], []
    for cls in spec.get("model", []):
        path = os.path.join(MODELS, cls + ".java")
        if not os.path.isfile(path):
            missing.append(cls)
            continue
        src = open(path, encoding="utf-8", errors="replace").read()
        name, bones = MJ.parse_constructor(src)
        origin, cubes = MJ.derive(bones)
        docs.append({
            "class": name,
            "source": os.path.relpath(path, ROOT).replace("\\", "/"),
            "modelClass": cls,
            "meshOriginVanilla": [round(v, 4) for v in origin],
            "bones": [{"name": b["name"],
                       "pivot": [round(v, 4) for v in (b["pivot"] or (0, 0, 0))],
                       "rotatedX": b["rotatedX"],
                       "cubes": b["cubes"]} for b in bones],
            "cubes": cubes,
            "modelBounds": union_bounds(cubes),
        })
    return docs, missing


def union_bounds(cubes):
    """The union of every cube's derived mesh extents, in model px (1 px = 1/16 block).

    This is what the MODEL measures -- with the beak and the legs included. It is not vanilla's
    hitbox; that comes from EntityX.setSize and is deliberately not invented here.
    """
    if not cubes:
        return None
    lo = [min(c["meshExtents"][0][i] for c in cubes) for i in range(3)]
    hi = [max(c["meshExtents"][1][i] for c in cubes) for i in range(3)]
    return {"lo": lo, "hi": hi, "size": [round(hi[i] - lo[i], 4) for i in range(3)]}


# ---------------------------------------------------------------------------------------------
# fetch

def fetch_mob(mob, spec, jar, outroot, force, install_dir):
    out = os.path.join(outroot, mob)
    tdir = os.path.join(out, "texture")
    os.makedirs(tdir, exist_ok=True)

    entry = {"mob": mob, "textures": [], "models": [], "missingModels": []}

    for jar_path in spec.get("textures", {}):
        engine_path = spec["textures"][jar_path]
        data = read_texture(jar, jar_path)
        if data is None:
            entry["textures"].append({"jar": jar_path, "engine": engine_path, "status": "ABSENT"})
            continue
        dest = os.path.join(tdir, os.path.basename(jar_path))
        if force or not os.path.isfile(dest):
            with open(dest, "wb") as f:
                f.write(data)
        entry["textures"].append({
            "jar": jar_path,
            "engine": engine_path,
            "file": os.path.relpath(dest, ROOT).replace("\\", "/"),
            "bytes": len(data),
            "sha256": sha256(data)[:16],
            "status": "ok",
        })
        if install_dir:
            target = os.path.join(install_dir, engine_path.replace("/", os.sep))
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, "wb") as f:
                f.write(data)

    docs, missing = parse_model(mob, spec)
    entry["models"] = docs
    entry["missingModels"] = missing
    if docs:
        with open(os.path.join(out, "model.json"), "w", encoding="utf-8") as f:
            json.dump({"mob": mob, "models": docs}, f, indent=2)
            f.write("\n")
    return entry


def fetch_loose_texture(jar_path, jar, outroot, force):
    jar_path = jar_path[len(JAR_IN_JAR):] if jar_path.startswith(JAR_IN_JAR) else jar_path
    data = read_texture(jar, jar_path)
    if data is None:
        raise SystemExit("no such texture in the jar: %s" % (JAR_IN_JAR + jar_path))
    dest = os.path.join(outroot, "_misc", os.path.basename(jar_path))
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    if force or not os.path.isfile(dest):
        with open(dest, "wb") as f:
            f.write(data)
    print("texture: %s -> %s (%d bytes)" % (JAR_IN_JAR + jar_path,
                                            os.path.relpath(dest, ROOT), len(data)))


# ---------------------------------------------------------------------------------------------
# reporting

def status_row(mob, spec, jar_names, outroot):
    have_tex = sum(1 for p in spec.get("textures", {}) if p in jar_names)
    n_tex = len(spec.get("textures", {}))
    classes = spec.get("model", [])
    have_mdl = [c for c in classes if os.path.isfile(os.path.join(MODELS, c + ".java"))]
    fetched = os.path.isdir(os.path.join(outroot, mob))
    return have_tex, n_tex, have_mdl, classes, fetched


def print_table(jar_path, jar, outroot):
    print("jar   : %s" % jar_path)
    print("out   : %s" % os.path.relpath(outroot, ROOT).replace("\\", "/"))
    print()
    print("%-17s %-9s %-9s %-8s %s" % ("mob", "textures", "model", "fetched", "source"))
    print("-" * 78)
    names = set(jar_entity_textures(jar))
    ready = 0
    for mob in sorted(MOBS):
        spec = MOBS[mob]
        have_tex, n_tex, have_mdl, classes, fetched = status_row(mob, spec, names, outroot)
        texx = "%d/%d" % (have_tex, n_tex)
        mdl = classes[0] if have_mdl else "-"
        if have_tex == n_tex and have_mdl:
            ready += 1
        print("%-17s %-9s %-9s %-8s %s" % (
            mob, texx, "yes" if have_mdl else "-", "yes" if fetched else "-",
            ", ".join(have_mdl) if have_mdl else "(no decompiled source on hand)"))
    print("-" * 78)
    print("%d/%d mobs have both their textures and a decompiled model source" % (ready, len(MOBS)))
    print()
    print("textures are extracted from the jar verbatim, so every mob's art is already available.")
    print("geometry needs a decompiled ModelX.java in tools/vanilla-models/; 1.7.3 ships obfuscated")
    print("classes, so those are collected separately. Add one and re-run to pick it up.")


def scan_assets(outroot):
    """Inventory everything actually sitting in the output dir.

    Rebuilt from disk rather than from the current run, so fetching one mob never erases the record
    of the others -- the manifest stays a true picture of what is on hand.
    """
    entries = []
    if not os.path.isdir(outroot):
        return entries
    for mob in sorted(os.listdir(outroot)):
        d = os.path.join(outroot, mob)
        if not os.path.isdir(d) or mob.startswith("_"):
            continue
        spec = MOBS.get(mob, {})
        engine_by_base = {os.path.basename(k): v for k, v in spec.get("textures", {}).items()}

        textures = []
        tdir = os.path.join(d, "texture")
        if os.path.isdir(tdir):
            for fn in sorted(os.listdir(tdir)):
                if not fn.lower().endswith(".png"):
                    continue
                data = open(os.path.join(tdir, fn), "rb").read()
                textures.append({"file": fn,
                                 "engine": engine_by_base.get(fn, fn),
                                 "bytes": len(data),
                                 "sha256": sha256(data)[:16]})

        models = []
        mj = os.path.join(d, "model.json")
        if os.path.isfile(mj):
            with open(mj, encoding="utf-8") as f:
                doc = json.load(f)
            for m in doc.get("models", []):
                models.append({"modelClass": m.get("modelClass"),
                               "source": m.get("source"),
                               "modelBounds": m.get("modelBounds")})

        entries.append({"mob": mob, "textures": textures, "models": models})
    return entries


def write_manifest(outroot, jar):
    entries = scan_assets(outroot)
    os.makedirs(outroot, exist_ok=True)
    doc = {
        "jar": jar,
        "note": ("textures are copied byte-for-byte from the vanilla 1.7.3 client jar; model "
                 "geometry is parsed from decompiled sources in tools/vanilla-models/. "
                 "modelBounds is what the model measures, not vanilla's entity hitbox."),
        "mobs": entries,
    }
    with open(os.path.join(outroot, "MANIFEST.json"), "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")

    n_tex = sum(len(e["textures"]) for e in entries)
    with_model = [e["mob"] for e in entries if e["models"]]
    lines = ["# Vanilla mob assets on hand", "",
             "Extracted from `%s`." % jar, "",
             "%d mobs, %d textures. Models: %s." % (
                 len(entries), n_tex,
                 ", ".join(with_model) if with_model else "none yet"),
             "",
             "| mob | textures | model | mesh bounds (px) | sha256 |",
             "|---|---|---|---|---|"]
    for e in entries:
        first = e["textures"][0] if e["textures"] else {}
        extra = len(e["textures"]) - 1
        tdesc = "%s%s" % (first.get("file", "-"), " (+%d)" % extra if extra > 0 else "")
        bounds = "-"
        if e["models"] and e["models"][0].get("modelBounds"):
            bounds = "%.0f x %.0f x %.0f" % tuple(e["models"][0]["modelBounds"]["size"])
        lines.append("| %s | %s | %s | %s | `%s` |" % (
            e["mob"], tdesc,
            ", ".join(m["modelClass"] for m in e["models"]) if e["models"] else "-",
            bounds, first.get("sha256", "-")))
    lines += ["",
              "`mesh bounds` is the model's own extent (beak, legs and all) in model px, "
              "1 px = 1/16 block. It is not vanilla's entity hitbox -- that lives in "
              "`EntityX.setSize` and is not invented here.", ""]
    with open(os.path.join(outroot, "MANIFEST.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print("manifest: %s (%d mobs, %d textures)"
          % (os.path.relpath(os.path.join(outroot, "MANIFEST.json"), ROOT), len(entries), n_tex))


# ---------------------------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", nargs="?", default="list",
                    choices=["list", "textures", "fetch"], help="what to do (default: list)")
    ap.add_argument("mobs", nargs="*", help="mob ids for fetch")
    ap.add_argument("--all", action="store_true", help="fetch every registered mob")
    ap.add_argument("--texture", help="fetch one loose texture by jar path (entity/...)")
    ap.add_argument("--jar", help="path to the 1.7.3 client jar")
    ap.add_argument("--out", default=ASSETS, help="output root (default tools/vanilla-assets)")
    ap.add_argument("--force", action="store_true", help="re-write files that already exist")
    ap.add_argument("--install", metavar="PACK_DIR",
                    help="also copy each texture to its engine path under this texture pack dir")
    args = ap.parse_args()

    jar_path = find_jar(args.jar)
    jar = open_jar(jar_path)

    if args.command == "textures":
        tex = jar_entity_textures(jar)
        print("%d entity textures in %s" % (len(tex), os.path.basename(jar_path)))
        print()
        for t in tex:
            print("  " + t)
        return 0

    if args.command == "list":
        print_table(jar_path, jar, args.out)
        return 0

    if args.texture:
        fetch_loose_texture(args.texture, jar, args.out, args.force)
        return 0

    wanted = sorted(MOBS) if args.all else args.mobs
    if not wanted:
        print("nothing to fetch: name some mobs, or pass --all")
        return 1

    unknown = [m for m in wanted if m not in MOBS]
    if unknown:
        raise SystemExit("unknown mob(s): %s\nrun `list` to see the registry"
                         % ", ".join(unknown))

    entries = []
    for mob in wanted:
        e = fetch_mob(mob, MOBS[mob], jar, args.out, args.force, args.install)
        entries.append(e)
        tex = "%d texture(s)" % len([t for t in e["textures"] if t["status"] == "ok"])
        mdl = ", ".join(m["modelClass"] for m in e["models"]) if e["models"] else "no model source"
        print("%-17s %-14s %s" % (mob, tex, mdl))

    write_manifest(args.out, jar_path)
    if args.install:
        print("installed textures under: %s" % args.install)
    return 0


if __name__ == "__main__":
    sys.exit(main())
