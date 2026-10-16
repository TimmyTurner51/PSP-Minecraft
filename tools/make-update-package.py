#!/usr/bin/env python3
"""
Build an update package: a zip of the files that changed, plus the manifest the installer reads out of
it, plus the update.txt the game checks against.

Why this exists
---------------
What ships is bigger than the EBOOT. The PBP carries only PARAM.SFO, two images and the PRX -- the
game reads `Assets/...` as ordinary loose files sitting next to it, and the compiled folder is about
130 MB of them beside a 4.9 MB EBOOT.PBP. So "push a build to the PSP" is not "replace EBOOT.PBP":
sending the whole folder over a handheld's wifi is minutes of transfer for one changed line of code.

The update unit is therefore a *patch*: only the files that actually differ, described by a manifest
that says exactly which paths it is allowed to touch, with a size and crc32 for each. That is what
makes a code fix about 5 MB, and what makes the installer able to tell a good download from a bad one.

Two files come out, and the split is deliberate:

  <outdir>/<name>.zip      the package. Its manifest.txt lives *inside* it, so a package cannot be
                           separated from a description of itself and a stale manifest next to a fresh
                           zip is impossible.
  <outdir>/update.txt      six lines the game fetches first: what version is available and where the
                           package is, with its size and crc32 so the download can be checked.

The wire format is line based and pipe separated rather than JSON, because PSP filenames may contain
spaces ("Nostalgia Craft") and because writing a correct JSON parser under this build's -fno-exceptions
would be a lot of code to get subtly wrong for no gain. See Scripts/Updater.h for the full reasoning.

What is deliberately never packaged
----------------------------------
Per-user and per-run files. A save is the player's, not the build's; overwriting it would be data loss,
and including it would make every package enormous. Log.txt is written on every boot. The staging files
the updater itself creates are excluded so a stale one cannot be packaged back into an update.

Usage
-----
  # A release: the full package, plus a patch for anyone on the version before it. This is the shape
  # to publish -- the full package is the only thing that can install from nothing, and the patch is
  # what stops an upgrade costing 95 MB when five files changed.
  python tools/make-update-package.py --version 3.0.2 \\
      --new "Minecraft - Compiled" \\
      --patch-base "releases/3.0.1" \\
      --outdir "update-server"

  # Everything that differs from a previous build -- the old behaviour, kept because a patch on its
  # own is what the repair flow and the --only test loop want. This makes the MAIN package a patch,
  # so it installs only onto 3.0.1:
  python tools/make-update-package.py --version 3.0.2 \\
      --base "releases/3.0.1" --new "Minecraft - Compiled" --outdir "update-server"

  # Or package the whole folder (first release to a card, or when in doubt):
  python tools/make-update-package.py --version 3.0.2 \\
      --new "Minecraft - Compiled" --outdir "update-server"

  # Pushing the build you are working on, which is the same version as the one on the console.
  # --force skips only the "is it newer" comparison; --only keeps the package small enough to test
  # the whole flow in seconds.
  python tools/make-update-package.py --version 3.0.2 --force \\
      --new "Minecraft - Compiled" --only "Assets/Textures/Minecraft/terrain.png" --outdir "update-server"

Then: python tools/serve-update.py --dir "update-server"

What comes out, and who each file is for
----------------------------------------
  <name>.zip                  the main package: full unless --base made it a patch
  <name>-from-<version>.zip   one per --patch-base, a patch only usable on that version
  update.txt                  the offer: the main package's keys, then one `patch` line per patch

The client picks between them by what it has installed, not by what the server prefers -- the server
cannot know, and a 95 MB download for five changed files is the cost of pretending it can. See
ChoosePackage in Scripts/Updater.h.

Checked
-------
Nothing here is machine-checked; it is a build tool. What it does do is refuse to write a package it
cannot describe: a file that cannot be read, a version that does not parse, or a diff that comes out
empty are all errors rather than silently producing a package that installs nothing (which the game
would then treat as "up to date" forever).
"""

import argparse
import fnmatch
import os
import sys
import zipfile
import zlib

# Never packaged. Save/ is the player's data. Log.txt is rewritten every boot. The rest are the
# updater's own scratch files -- packaging update.zip into an update would be a fine way to make the
# next install recursive, and packaging a leftover handoff file (mcupdater-pending.txt) would make a
# fresh install think there was already an update pending. UpdaterLog.txt is a per-run record.
#
# UpdaterLog.txt rather than the game's Log.txt because the game's boot truncates Log.txt in a debug
# build -- see Update::UpdaterLogName() in Scripts/Updater.h, which is where the name is defined. It is
# listed here by its literal value because this script cannot include a C++ header.
#
# Config/, perfprobe.lmsdd/, trace.txt and boot.txt are the RUNTIME paths: the game rewrites them while
# it runs, so a package that shipped them would describe a state the install cannot stay in, and the
# integrity check would report files that were never wrong. THIS LIST MUST STAY IN STEP WITH
# Update::IsRuntimePath in Scripts/Updater.cpp, which excludes the same paths from the check -- two
# lists, one decision, and a file that is in one and not the other is a repair prompt nobody can clear.
#
# .svn is version-control cruft that has no business on a memory stick. It was swept into a release
# before this was written -- 78 entries of it -- which the index read is what made visible.
EXCLUDED_DIRS = {"Save", "Config", "perfprobe.lmsdd", ".svn"}
EXCLUDED_FILES = {"Log.txt", "UpdaterLog.txt", "trace.txt", "boot.txt",
                  "mcupdater-pending.txt",
                  "update.zip", "update.txt", "update_url.txt", "manifest.txt"}
EXCLUDED_SUFFIXES = (".lmsd", ".lmsdd", ".tmp", ".bak")


def version_code(version):
    """'3.2.10' -> 30210. Must match MC_VERSION_CODE in Scripts/Version.h.

    The integer exists so nothing ever compares the strings: '3.0.10' sorts before '3.0.9' as text.
    """
    parts = version.split(".")
    if len(parts) != 3:
        raise ValueError(f"version must be MAJOR.MINOR.PATCH, got {version!r}")
    try:
        major, minor, patch = (int(p) for p in parts)
    except ValueError:
        raise ValueError(f"version must be MAJOR.MINOR.PATCH with numbers, got {version!r}")
    for name, value, limit in (("major", major, 99), ("minor", minor, 99), ("patch", patch, 99)):
        if value < 0 or value > limit:
            raise ValueError(f"{name} must be 0..{limit}, got {value}")
    return major * 10000 + minor * 100 + patch


def detect_base_version(base_dir):
    """The version a --base folder holds, or None if it cannot be established.

    Tried in order of trustworthiness:

      1. `install.txt` in the folder, which the launcher writes next to an installed game and which
         states the version outright. This is the case that matters -- diffing against a build pulled
         off a card -- and it is authoritative.
      2. The folder's own name, when it parses as a version ("releases/3.0.1", "Installs/3.0.0").

    Returns None rather than guessing. A wrong base version is worse than a missing one: a missing one
    only means the client cannot check, while a wrong one makes the client refuse a patch that would
    have applied cleanly, with an error naming a version that is not the problem.
    """
    install_txt = os.path.join(base_dir, "install.txt")
    if os.path.isfile(install_txt):
        try:
            with open(install_txt, "r") as handle:
                for line in handle:
                    key, _, value = line.strip().partition("|")
                    if key == "version" and value:
                        return value
        except OSError:
            pass

    name = os.path.basename(os.path.normpath(base_dir))
    try:
        version_code(name)
        return name
    except ValueError:
        return None


def is_packaged(relpath, only=None):
    """True if a path relative to the game root belongs in a package.

    `only` is an optional list of fnmatch patterns; when given, a path must match one of them. It
    exists mainly for testing: a package that contains EBOOT.PBP cannot be installed by the running
    game (see Scripts/Updater.h), so the only way to exercise the full in-place install on a real
    console without the Updater EBOOT is to package an asset or two.
    """
    if only:
        if not any(fnmatch.fnmatch(relpath, pattern) for pattern in only):
            return False
    parts = relpath.split("/")
    if parts[0] in EXCLUDED_DIRS:
        return False
    if parts[-1] in EXCLUDED_FILES:
        return False
    if parts[-1].endswith(EXCLUDED_SUFFIXES):
        return False
    return True


def scan(root, only=None):
    """Map relative path -> (size, crc32) for every packageable file under root."""
    found = {}
    for dirpath, dirnames, filenames in os.walk(root):
        # Prune in place so os.walk does not descend into an excluded directory at all.
        dirnames[:] = [d for d in dirnames if d not in EXCLUDED_DIRS]
        for name in filenames:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            if not is_packaged(rel, only):
                continue
            try:
                with open(full, "rb") as handle:
                    data = handle.read()
            except OSError as error:
                raise SystemExit(f"error: cannot read {full}: {error}")
            found[rel] = (len(data), zlib.crc32(data) & 0xFFFFFFFF)
    return found


def diff_against(new_files, base_files):
    """-> (changed, removed) describing new_files relative to base_files.

    `base_files` of None means there is no base, i.e. the package carries everything. That is the
    full-install case, not an empty diff.
    """
    if base_files is None:
        return sorted(new_files), []
    changed = sorted(p for p, meta in new_files.items() if base_files.get(p) != meta)
    removed = sorted(p for p in base_files if p not in new_files)
    return changed, removed


def build_package(outdir, name, version, code, new_root, new_files, base_files, base_version,
                  force, label):
    """Write one package and return (path, size, crc, full, changed, removed).

    `base_files` is None for a full package or a dict of path -> (size, crc) for a patch. `label` only
    appears in the refusal below, so the message can say which package came out empty when a release
    builds more than one.
    """
    changed, removed = diff_against(new_files, base_files)
    full = base_files is None

    if not changed and not removed:
        raise SystemExit(
            f"error: nothing changed, so there is no update to build ({label}).\n"
            "       A package with no files is refused by the game as well -- installing it would\n"
            "       report success while changing nothing, and the game would then believe it was up\n"
            "       to date forever. Bump the version only when something actually changed.")

    # Force and full/base come first so the description reads before the contents, matching the order
    # the C++ parser and a human both expect.
    manifest = [f"version|{version}", f"code|{code}"]
    if force:
        # Inside the package as well as in the offer, so the manifest stays self-describing: the
        # Updater EBOOT reads it with no server to ask.
        manifest.append("force|1")
    if full:
        # No base means this package carries EVERY packageable file, not a diff. Saying so lets a
        # launcher extract it into an empty install folder instead of first copying an existing
        # install to patch -- which is the difference between a few seconds and a copy of the whole
        # 135 MB tree. A patch does not carry this line, and a reader that does not know the key
        # treats the package as a patch, which is the safe direction to be wrong in.
        manifest.append("full|1")
    else:
        # The version this diff was taken against, so the client can refuse to apply it to anything
        # else. Without this a patch is "some changes" with no idea what they are changes TO, and
        # applying it to the wrong tree produces a mixture that half works -- the worst outcome,
        # because nothing downstream can tell that it is wrong.
        manifest.append(f"base|{base_version}")
    for path in changed:
        size, crc = new_files[path]
        manifest.append(f"file|{path}|{size}|{crc}")
    for path in removed:
        manifest.append(f"delete|{path}")

    path = os.path.join(outdir, name)
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as archive:
        # The manifest goes in first so it is the first entry in the archive. Nothing depends on the
        # order, but it makes `unzip -l` on a package show the description before the contents.
        archive.writestr("manifest.txt", "\n".join(manifest) + "\n")
        for entry in changed:
            archive.write(os.path.join(new_root, entry.replace("/", os.sep)), entry)

    with open(path, "rb") as handle:
        blob = handle.read()
    return path, len(blob), zlib.crc32(blob) & 0xFFFFFFFF, full, changed, removed


def main():
    parser = argparse.ArgumentParser(
        description="Build an update package (zip + manifest) and the update.txt that describes it.")
    parser.add_argument("--version", required=True,
                        help="the version this package installs, as MAJOR.MINOR.PATCH")
    parser.add_argument("--new", required=True,
                        help="the compiled game folder to package (e.g. 'Minecraft - Compiled')")
    parser.add_argument("--base", default=None,
                        help="a previous build to diff against. Omit to package everything.")
    parser.add_argument("--patch-base", action="append", metavar="DIR", default=None,
                        help="ALSO build a patch against this build and advertise it as a `patch` "
                             "entry, leaving the main package as whatever --base made it (a full "
                             "install when --base is omitted). Repeatable. This is how a release "
                             "offers a small download to clients on an older version while still "
                             "offering the full package to everyone else.")
    parser.add_argument("--base-version", default=None,
                        help="the version --base holds, recorded in the package so a client can refuse "
                             "to apply it to anything else. Detected from the base folder's install.txt "
                             "or its name when omitted; without either, the package is built without a "
                             "declared base and cannot be checked by the client.")
    parser.add_argument("--outdir", required=True,
                        help="where to write the package and update.txt (a folder, created if absent)")
    parser.add_argument("--name", default=None,
                        help="package filename (default MineCraft-<version>.zip)")
    parser.add_argument("--list", action="store_true",
                        help="print the file list and exit without writing anything")
    parser.add_argument("--only", action="append", metavar="PATTERN",
                        help="package only paths matching this glob; repeatable. Mainly for testing: "
                             "a package without EBOOT.PBP is one the running game can install itself")
    parser.add_argument("--force", action="store_true",
                        help="mark the offer as forced, so the game offers it even when its version is "
                             "not newer than the one running. This is the testing case: the build you "
                             "want to push is the one you just made, so the server advertises the same "
                             "version the console already has and a plain comparison says 'up to date'")
    parser.add_argument("--launcher", action="store_true",
                        help="build a package for the LAUNCHER's own update, not a game install. --new "
                             "is then the launcher's own folder (it contains EBOOT.PBP, Assets/, "
                             "Versions.txt and Updater/EBOOT.PBP), and the offer gets a `target|launcher` "
                             "line. Must be a full package: the launcher writes its files in place over "
                             "itself, so there is nothing to diff against, and a package that carried a "
                             "game install's manifest paths would be refused by the launcher")
    args = parser.parse_args()

    try:
        code = version_code(args.version)
    except ValueError as error:
        raise SystemExit(f"error: {error}")

    if not os.path.isdir(args.new):
        raise SystemExit(f"error: --new is not a folder: {args.new}")

    # A launcher package is full by construction, and saying so here rather than letting it through is
    # the point: a launcher update is applied IN PLACE over the program that is running it, so a diff
    # against some base has nothing to be applied to -- there is no second copy of the launcher to diff
    # into, the way Installs/<version> is a second copy of a game install. Refusing beats building a
    # package the launcher will download and then reject.
    if args.launcher and (args.base or args.patch_base or args.base_version):
        raise SystemExit("error: --launcher packages are always full; drop --base/--patch-base/--base-version")

    if args.launcher:
        name = args.name or f"MineCraftLauncher-{args.version}.zip"
    else:
        name = args.name or f"MineCraft-{args.version}.zip"

    new_files = scan(args.new, args.only)
    if not new_files:
        if args.only:
            raise SystemExit(f"error: nothing under {args.new} matches {args.only}")
        raise SystemExit(f"error: no packageable files under {args.new}")

    # base_files None means "carry everything"; a dict means "diff against this". Keeping None rather
    # than an empty dict is what lets a full package and a patch be told apart all the way down, since
    # an empty dict would read as "the base has no files, so everything is new" -- a full package that
    # claims to be a patch.
    if args.base:
        if not os.path.isdir(args.base):
            raise SystemExit(f"error: --base is not a folder: {args.base}")
        base_files = scan(args.base, args.only)

        # Which version the diff was taken against, so the client can check it before applying. The
        # explicit flag wins; otherwise it is read off the folder. Failing to establish it is a warning
        # rather than an error -- a package with no declared base is still correct, just unverifiable --
        # but it is said out loud, because silent is how a patch ends up applied to the wrong tree.
        base_version = args.base_version or detect_base_version(args.base)
        if base_version:
            try:
                version_code(base_version)
            except ValueError:
                raise SystemExit(f"error: --base-version is not MAJOR.MINOR.PATCH: {base_version!r}")
        else:
            print(f"warning: could not tell which version {args.base} is, so the package will not\n"
                  f"         declare a base and a client cannot check it before applying. Pass\n"
                  f"         --base-version to fix that.", file=sys.stderr)
    else:
        base_files = None
        base_version = None

    # The patches to build alongside the main package. Each has to be able to name the version it
    # applies to, and here that is required rather than advised: the base is what the client selects
    # the entry BY, so a patch with no base could never be chosen -- it would be an asset nothing can
    # ever download. The main package is allowed to leave it unstated; this is not.
    patch_bases = []
    for patch_dir in (args.patch_base or []):
        if not os.path.isdir(patch_dir):
            raise SystemExit(f"error: --patch-base is not a folder: {patch_dir}")
        patch_version = detect_base_version(patch_dir)
        if not patch_version:
            raise SystemExit(
                f"error: cannot tell which version {patch_dir} is, so a patch against it could not\n"
                f"       be advertised -- the client picks a patch by the version it applies to, so\n"
                f"       that version has to be known. Put an install.txt in it, or name the folder\n"
                f"       after the version (releases/3.0.1).")
        try:
            version_code(patch_version)
        except ValueError:
            raise SystemExit(f"error: {patch_dir} looks like version {patch_version!r}, which is not "
                             f"MAJOR.MINOR.PATCH")
        patch_bases.append((patch_dir, patch_version))

    if args.list:
        changed, removed = diff_against(new_files, base_files)
        print(f"version {args.version} ({code})")
        print(f"{len(changed)} file(s) changed" + (f", {len(removed)} removed" if removed else ""))
        for path in changed:
            size, crc = new_files[path]
            was = (base_files or {}).get(path)
            note = "new" if was is None else f"was {was[0]} bytes"
            print(f"  {path}  {size} bytes  crc {crc:08x}  ({note})")
        for path in removed:
            print(f"  {path}  DELETE")
        for patch_dir, patch_version in patch_bases:
            changed, removed = diff_against(new_files, scan(patch_dir, args.only))
            print(f"patch from {patch_version}: {len(changed)} file(s)"
                  + (f", {len(removed)} removed" if removed else ""))
        return 0

    os.makedirs(args.outdir, exist_ok=True)

    zip_path, zip_size, zip_crc, is_full, changed, removed = build_package(
        args.outdir, name, args.version, code, args.new, new_files, base_files, base_version,
        args.force, "the main package")

    # The patches, named so the version they upgrade FROM is readable in the filename. That is not
    # decoration: on a release page holding several assets, the filename is the only thing that says
    # which download is the small one and who it is for.
    stem = os.path.splitext(name)[0]
    patches = []
    for patch_dir, patch_version in patch_bases:
        patch_name = f"{stem}-from-{patch_version}.zip"
        patch_path, patch_size, patch_crc, _full, patch_changed, patch_removed = build_package(
            args.outdir, patch_name, args.version, code, args.new, new_files,
            scan(patch_dir, args.only), patch_version, args.force,
            f"the patch from {patch_version}")
        patches.append((patch_version, patch_name, patch_size, patch_crc,
                        len(patch_changed), len(patch_removed)))

    # The update.txt the game fetches. Same keys ParseOffer expects for the main package, then one
    # `patch|file|size|crc|base` line per patch. The client picks between them by what it has
    # installed; see ChoosePackage in Scripts/Updater.h for that side of it.
    #
    # `full` and `base` are mirrored from the manifest, and that duplication is the point: the
    # manifest is inside the zip, so without them the only way to learn whether an offer is a 5 MB
    # patch or a 95 MB full install is to download all of it -- which is the wrong order when the
    # answer decides whether the download should happen.
    offer_text = (f"version|{args.version}\n"
                  f"code|{code}\n"
                  f"file|{name}\n"
                  f"size|{zip_size}\n"
                  f"crc|{zip_crc}\n")
    if args.launcher:
        # What turns this offer into the launcher's self-update on the client. See Offer::launcher in
        # Scripts/Updater.h: it changes which installer the launcher walks into, and nothing else about
        # the protocol -- the manifest, the size and crc checks and the delete list are all identical.
        offer_text += "target|launcher\n"
    if args.force:
        # Only written when asked for. The game defaults this to false, so a release package built
        # without --force can never be forced by a stale field.
        offer_text += "force|1\n"
    if is_full:
        offer_text += "full|1\n"
    elif base_version:
        offer_text += f"base|{base_version}\n"
    for patch_version, patch_name, patch_size, patch_crc, _c, _r in patches:
        offer_text += f"patch|{patch_name}|{patch_size}|{patch_crc}|{patch_version}\n"

    offer_path = os.path.join(args.outdir, "update.txt")
    with open(offer_path, "w", newline="\n") as handle:
        handle.write(offer_text)

    total_new = sum(new_files[p][0] for p in changed)

    print(f"version    {args.version} ({code})" + ("   [FORCED: will be offered even if not newer]"
                                                    if args.force else ""))
    # Which kind of package this is, said plainly, because the size difference between a patch and a
    # full install is two orders of magnitude and the two are otherwise indistinguishable in the output.
    if base_files is not None:
        print(f"kind       patch against {base_version or 'an unnamed base'}"
              + ("" if base_version else "   [no base recorded: clients cannot check it]"))
    else:
        print(f"kind       full install (carries every file)")
    print(f"changed    {len(changed)} file(s), {total_new} bytes uncompressed"
          + (f"; {len(removed)} deletion(s)" if removed else ""))
    print(f"package    {zip_path} ({zip_size} bytes, crc {zip_crc:08x})")
    for patch_version, patch_name, patch_size, patch_crc, patch_changed, patch_removed in patches:
        print(f"patch      {patch_name} ({patch_size} bytes, crc {patch_crc:08x})"
              f"  from {patch_version}: {patch_changed} file(s)"
              + (f", {patch_removed} deletion(s)" if patch_removed else ""))
        if patch_changed or patch_removed:
            if patch_size < zip_size:
                print(f"           clients on {patch_version} download {patch_size} bytes "
                      f"instead of {zip_size}")
            else:
                # Worth saying out loud rather than printing a saving that is not there: a patch no
                # smaller than the full package means the two builds barely share anything, so this
                # base buys the client nothing and a release should question whether it belongs.
                print(f"           NOTE: this patch is not smaller than the full package "
                      f"({patch_size} vs {zip_size} bytes), so clients on {patch_version} save "
                      f"nothing by taking it")
    print(f"offer      {offer_path}")
    print()
    print("Serve it with:  python tools/serve-update.py --dir " + args.outdir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
