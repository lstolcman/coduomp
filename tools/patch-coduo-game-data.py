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
    polish      Replace the invalid Polish key token [{+aktywacja}] with the
                engine command [{+activate}] in the shipped localization.
    rconmenu    Join the unterminated quoted string in the bundled rcon menu's
                about page. The literal newline makes the menu parser reject
                the whole item, so the page never loads.
    polishquotes
                Remove quotation marks that the Polish and French translations
                doubled by mistake. The string loader strips only one leading
                and one trailing quote, so the extra quote is displayed.

Usage:

    tools/patch-coduo-game-data.py --game-root /path/to/coduo
    tools/patch-coduo-game-data.py --game-root /path/to/coduo --apply --backup

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

POLISH_BAD = b"[{+aktywacja}]"
POLISH_GOOD = b"[{+activate}]"
POLISH_ENTRY_COUNTS = {
    "re.str": 2,
    "gmi_bas.str": 2,
    "gmi_game.str": 2,
    "gmi_vehicle.str": 10,
}

RCON_MENU_BAD = b'\t\t\ttext\t\t\t"Special thanks to mnml *\r\n"'
RCON_MENU_GOOD = b'\t\t\ttext\t\t\t"Special thanks to mnml *"'

# The .str loader (CStringEdPackage::InsideQuotes) strips at most one leading
# and one trailing quote, so a doubled quote at either end is shown to the
# player. Each entry below is a literal the shipped translation stores with
# that extra quote.
QUOTE_FIXES = (
    (
        "localizedstrings/polish/gmi_cgame.str",
        b'""Naci\x9cnij [%s], aby u\xbfy\xe6 kaemu kaliber 50""',
        b'"Naci\x9cnij [%s], aby u\xbfy\xe6 kaemu kaliber 50"',
    ),
    (
        "localizedstrings/polish/gmi_cgame.str",
        b'""Naci\x9cnij [%s], aby u\xbfy\xe6 DP28""',
        b'"Naci\x9cnij [%s], aby u\xbfy\xe6 DP28"',
    ),
    (
        "localizedstrings/polish/gmi_cgame.str",
        b'""Naci\x9cnij [%s], aby u\xbfy\xe6 SG43""',
        b'"Naci\x9cnij [%s], aby u\xbfy\xe6 SG43"',
    ),
    (
        "localizedstrings/polish/gmi_cgame.str",
        b'""Naci\x9cnij [%s], aby u\xbfy\xe6 kaemu kaliber 30""',
        b'"Naci\x9cnij [%s], aby u\xbfy\xe6 kaemu kaliber 30"',
    ),
    (
        "localizedstrings/polish/gmi_cgame.str",
        b'""Naci\x9cnij [%s], aby u\xbfy\xe6 MG34""',
        b'"Naci\x9cnij [%s], aby u\xbfy\xe6 MG34"',
    ),
    (
        "localizedstrings/polish/gmi_ctf.str",
        b'""%s upu\x9cci\xb3 flag\xea do wody. "',
        b'"%s upu\x9cci\xb3 flag\xea do wody. "',
    ),
    (
        "localizedstrings/polish/gmi_script.str",
        b'""Naci\x9cnij [%s], aby zabra\xe6 wa\xbfne dokumenty."',
        b'"Naci\x9cnij [%s], aby zabra\xe6 wa\xbfne dokumenty."',
    ),
    (
        "localizedstrings/polish/pi_ponyri.str",
        b'radzieckiej ziemi!""',
        b'radzieckiej ziemi!"',
    ),
    (
        "localizedstrings/french/re_obj.str",
        b'le camp alli\xe9.""',
        b'le camp alli\xe9."',
    ),
)

FIX_NAMES = ("retrieval", "polish", "rconmenu", "polishquotes")
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


def plan_retrieval(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir() or not info.filename.lower().endswith(
            "gametypes/re.gsc"
        ):
            continue
        data = archive.read(info)
        old_count = data.count(RETRIEVAL_OLD)
        new_count = data.count(RETRIEVAL_NEW)
        if new_count == 1 and old_count == 0:
            continue
        if old_count != 1:
            raise ValueError(
                f"{label}: {info.filename}: expected exactly one RETRIEVAL "
                f"guard, found {old_count} (already patched: {new_count})"
            )
        replacements[info.filename] = data.replace(
            RETRIEVAL_OLD, RETRIEVAL_NEW, 1
        )
    return replacements


def plan_polish(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        lower = info.filename.lower()
        if info.is_dir() or not lower.startswith("localizedstrings/polish/"):
            continue
        if not lower.endswith(".str"):
            continue
        data = archive.read(info)
        bad_count = data.count(POLISH_BAD)
        if bad_count == 0:
            continue

        base = lower.rsplit("/", 1)[-1]
        expected = POLISH_ENTRY_COUNTS.get(base)
        if expected is None:
            raise ValueError(
                f"{label}: {info.filename}: unexpected file with the invalid "
                f"Polish key token ({bad_count} occurrences)"
            )
        if bad_count != expected:
            raise ValueError(
                f"{label}: {info.filename}: expected {expected} invalid "
                f"Polish key tokens, found {bad_count}"
            )
        replacements[info.filename] = data.replace(POLISH_BAD, POLISH_GOOD)
    return replacements


def plan_rconmenu(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir():
            continue
        lower = info.filename.lower()
        if not lower.endswith("ui_mp/scriptmenus/about.menu"):
            continue
        data = archive.read(info)
        bad_count = data.count(RCON_MENU_BAD)
        if bad_count == 0:
            continue
        if bad_count != 1:
            raise ValueError(
                f"{label}: {info.filename}: expected one unterminated rcon "
                f"menu string, found {bad_count}"
            )
        replacements[info.filename] = data.replace(
            RCON_MENU_BAD, RCON_MENU_GOOD, 1
        )
    return replacements


def plan_polishquotes(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir():
            continue
        lower = info.filename.lower()
        group = [
            (old, new)
            for suffix, old, new in QUOTE_FIXES
            if lower.endswith(suffix)
        ]
        if not group:
            continue
        data = archive.read(info)
        counts = [data.count(old) for old, _ in group]
        if all(count == 0 for count in counts):
            continue
        if any(count != 1 for count in counts):
            raise ValueError(
                f"{label}: {info.filename}: expected each quoted entry once, "
                f"found {counts}"
            )
        for old, new in group:
            data = data.replace(old, new, 1)
        replacements[info.filename] = data
    return replacements


PLANNERS = {
    "retrieval": (
        "RETRIEVAL guard fix",
        plan_retrieval,
    ),
    "polish": (
        "Polish use-key token fix",
        plan_polish,
    ),
    "rconmenu": (
        "rcon menu about-page string fix",
        plan_rconmenu,
    ),
    "polishquotes": (
        "Polish and French stray-quote fix",
        plan_polishquotes,
    ),
}


def patch_archive(path: str, fix: str, apply: bool, backup: bool):
    """Apply one fix to one archive. Returns 'patched', 'planned', or None."""
    _, planner = PLANNERS[fix]
    with zipfile.ZipFile(path, "r") as archive:
        replacements = planner(archive, path)
        names = archive.namelist()

    if not replacements:
        return None

    for entry in sorted(replacements):
        print(f"  {path}: {entry}: will patch")
    if not apply:
        print(f"  {path}: dry run, not written")
        return "planned"

    before = sha256_file(path)
    if backup:
        backup_path = path + ".bugfix-backup"
        if not os.path.exists(backup_path):
            shutil.copy2(path, backup_path)
            print(f"  {path}: backup {backup_path}")

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
                elif info.filename in replacements:
                    destination.writestr(new_info, replacements[info.filename])
                else:
                    with source.open(info, "r") as src, destination.open(
                        new_info, "w"
                    ) as dst:
                        shutil.copyfileobj(src, dst, 1 << 20)

        with zipfile.ZipFile(temporary, "r") as check:
            bad = check.testzip()
            if bad is not None:
                raise ValueError(f"{path}: CRC failure in {bad}")
            if check.namelist() != names:
                raise ValueError(f"{path}: entry list changed during rewrite")
            if planner(check, path):
                raise ValueError(f"{path}: post-write verification failed")

        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.remove(temporary)

    after = sha256_file(path)
    print(f"  {path}: patched")
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
    archives = list(iter_archives(game_root))

    exit_code = 0
    for fix in requested:
        title, _ = PLANNERS[fix]
        print(title)
        patched = 0
        planned = 0
        for archive in archives:
            try:
                result = patch_archive(archive, fix, args.apply, args.backup)
                if result == "patched":
                    patched += 1
                elif result == "planned":
                    planned += 1
            except (OSError, ValueError, zipfile.BadZipFile) as error:
                print(f"error: {error}", file=sys.stderr)
                exit_code = 1

        if planned != 0:
            print(f"{planned} archive(s) would be changed; rerun with --apply")
        elif patched == 0:
            print("no archive needed a change")
        else:
            print(f"{patched} archive(s) patched")
        if exit_code != 0:
            return exit_code
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
