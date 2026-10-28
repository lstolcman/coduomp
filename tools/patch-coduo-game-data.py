#!/usr/bin/env python3
"""Patch shipped Call of Duty game data defects in an installed game tree.

The tool operates on PK3 archives in place. It is a dry run unless --apply is
given, and it refuses to write when an archive does not contain exactly the
expected number of target strings. Each rewritten archive is re-read and
CRC-checked before it replaces the original.

Supported fixes:

    retrieval   Re-arm a RETRIEVAL objective when it is used before the match
                starts. The stock script returns from retrieval_think(), which
                permanently disables the objective after one early use.

Usage:

    tools/patch-coduo-game-data.py --game-root /path/to/coduo
    tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix retrieval --apply

The game root is the directory that contains Main/ and uo/. Downloaded server
data below server-cache/ is intentionally left untouched.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import sys
import zipfile

RETRIEVAL_OLD = b'\t\tif(!game["matchstarted"])\r\n\t\t\treturn;'
RETRIEVAL_NEW = (
    b'\t\tif(!game["matchstarted"])\r\n\t\t{\r\n'
    b'\t\t\twait 0.5;\r\n\t\t\tcontinue;\r\n\t\t}'
)

FIX_NAMES = ("retrieval",)
SKIP_DIRECTORIES = {"server-cache"}


def sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def iter_archives(game_root: str):
    for dirpath, dirnames, filenames in os.walk(game_root):
        dirnames[:] = sorted(
            name for name in dirnames if name.lower() not in SKIP_DIRECTORIES
        )
        for filename in sorted(filenames):
            if filename.lower().endswith(".pk3"):
                yield os.path.join(dirpath, filename)


def scan_archive(path: str):
    """Return (entry names, target entry states)."""
    targets = []
    with zipfile.ZipFile(path, "r") as archive:
        names = archive.namelist()
        for info in archive.infolist():
            if info.is_dir():
                continue
            if not info.filename.lower().endswith("gametypes/re.gsc"):
                continue
            data = archive.read(info)
            old_count = data.count(RETRIEVAL_OLD)
            new_count = data.count(RETRIEVAL_NEW)
            targets.append((info.filename, old_count, new_count))
    return names, targets


def patch_archive(path: str, apply: bool, backup: bool):
    """Patch one archive. Returns 'patched', 'planned', or None."""
    names, targets = scan_archive(path)
    if not targets:
        return None

    rel = path
    pending = []
    for entry, old_count, new_count in targets:
        if new_count == 1 and old_count == 0:
            print(f"  {rel}: {entry}: already patched")
            continue
        if old_count != 1:
            raise ValueError(
                f"{rel}: {entry}: expected exactly one RETRIEVAL guard, "
                f"found {old_count} (already patched: {new_count})"
            )
        pending.append(entry)
        print(f"  {rel}: {entry}: will replace the early-return guard")

    if not pending:
        return None
    if not apply:
        print(f"  {rel}: dry run, not written")
        return "planned"

    before = sha256_file(path)
    if backup:
        backup_path = path + ".bugfix-backup"
        if not os.path.exists(backup_path):
            shutil.copy2(path, backup_path)
            print(f"  {rel}: backup {backup_path}")

    temporary = path + ".bugfix-tmp"
    try:
        with zipfile.ZipFile(path, "r") as source, zipfile.ZipFile(
            temporary, "w"
        ) as destination:
            for info in source.infolist():
                new_info = zipfile.ZipInfo(info.filename, info.date_time)
                new_info.compress_type = info.compress_type
                new_info.external_attr = info.external_attr
                new_info.internal_attr = info.internal_attr
                new_info.create_system = info.create_system
                new_info.comment = info.comment

                if info.is_dir():
                    destination.writestr(new_info, b"")
                    continue
                if info.filename not in pending:
                    with source.open(info, "r") as src, destination.open(
                        new_info, "w"
                    ) as dst:
                        shutil.copyfileobj(src, dst, 1 << 20)
                    continue

                data = source.read(info)
                if data.count(RETRIEVAL_OLD) != 1:
                    raise ValueError(
                        f"{rel}: {info.filename}: guard changed during rewrite"
                    )
                destination.writestr(
                    new_info, data.replace(RETRIEVAL_OLD, RETRIEVAL_NEW, 1)
                )

        with zipfile.ZipFile(temporary, "r") as check:
            bad = check.testzip()
            if bad is not None:
                raise ValueError(f"{rel}: CRC failure in {bad}")
            for entry in pending:
                data = check.read(entry)
                if data.count(RETRIEVAL_NEW) != 1 or RETRIEVAL_OLD in data:
                    raise ValueError(
                        f"{rel}: {entry}: post-write verification failed"
                    )
            if check.namelist() != names:
                raise ValueError(f"{rel}: entry list changed during rewrite")

        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.remove(temporary)

    after = sha256_file(path)
    print(f"  {rel}: patched")
    print(f"    sha256 before: {before}")
    print(f"    sha256 after:  {after}")
    return "patched"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Patch shipped Call of Duty game data defects in place."
    )
    parser.add_argument(
        "--game-root",
        required=True,
        help="directory that contains the Main/ and uo/ game data",
    )
    parser.add_argument(
        "--fix",
        action="append",
        choices=FIX_NAMES,
        default=None,
        help="fix to apply; may be repeated (default: all)",
    )
    parser.add_argument(
        "--apply",
        action="store_true",
        help="write the patched archives (default: report only)",
    )
    parser.add_argument(
        "--backup",
        action="store_true",
        help="keep a .bugfix-backup copy beside each modified archive",
    )
    args = parser.parse_args()

    game_root = os.path.abspath(args.game_root)
    if not os.path.isdir(os.path.join(game_root, "Main")):
        parser.error(f"{game_root} does not contain a Main/ directory")

    requested = tuple(args.fix) if args.fix else FIX_NAMES
    if requested != ("retrieval",):
        parser.error("only the retrieval fix is implemented by this revision")

    print("RETRIEVAL guard fix")
    patched = 0
    planned = 0
    for archive in iter_archives(game_root):
        try:
            result = patch_archive(archive, args.apply, args.backup)
            if result == "patched":
                patched += 1
            elif result == "planned":
                planned += 1
        except (OSError, ValueError, zipfile.BadZipFile) as error:
            print(f"error: {error}", file=sys.stderr)
            return 1

    if planned != 0:
        print(f"{planned} archive(s) would be changed; rerun with --apply")
    elif patched == 0:
        print("no archive needed a change")
    else:
        print(f"{patched} archive(s) patched")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
