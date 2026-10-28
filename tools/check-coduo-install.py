#!/usr/bin/env python3
"""Check that a Call of Duty: United Offensive tree has the client runtime files.

The client is not contained in Main/ and uo/. Copying only those two
directories produces a tree that starts and renders maps, yet loses the
menu backdrop, sound, movies, CD-key authorization, and the base-game modules.
This tool reports which support files are absent and, when the selected
language has no localized UO pak, explains the missing menu art.

Usage:

    tools/check-coduo-install.py --game-root /path/to/coduo
    tools/check-coduo-install.py --game-root /path/to/copied \\
        --reference /path/to/complete

Exit status is 1 when a required file is missing, the selected language has no
localized pak set, or --reference finds an absent support file. It is 0 for
warnings only. Nothing is written.
"""

from __future__ import annotations

import argparse
import os
import sys

LANGUAGE_NAMES = (
    "english",
    "french",
    "german",
    "italian",
    "spanish",
    "british",
    "russian",
    "polish",
    "korean",
    "taiwanese",
    "japanese",
    "chinese",
    "thai",
    "leet",
)

# Required for the recovered Windows multiplayer client to start and join a
# UO server. The module loader searches uo/, Main/, and the working directory.
REQUIRED = (
    "CoDUOMP.exe",
    "version.inf",
    "Main/pak0.pk3",
    "uo/pakuo00.pk3",
    "uo/uo_cgame_mp_x86.dll",
    "uo/uo_game_mp_x86.dll",
    "uo/uo_ui_mp_x86.dll",
)

# Shipped with the retail client. The recovered client degrades without some
# of these, but a complete installation has all of them.
RECOMMENDED = (
    "localization.txt",
    "mss32.dll",
    "miles/mssmp3.asi",
    "miles/mssdsp.flt",
    "validatekey.dll",
    "EaxMan.dll",
    "binkw32.dll",
    "cod.bmp",
    "coduo.bmp",
    "Main/config_mp.cfg",
    "uo/uoconfig_mp.cfg",
    "Main/cgame_mp_x86.dll",
    "Main/game_mp_x86.dll",
    "Main/ui_mp_x86.dll",
    "cgamex86.dll",
    "gamex86.dll",
    "uix86.dll",
    "uo_cgamex86.dll",
    "uo_gamex86.dll",
    "uo_uix86.dll",
)

# Files whose absence is normal for a fresh or copied tree: they are logs,
# regenerated caches, user backups, or per-machine state.
IGNORED_COMPARE = (
    ".bugfix-backup",
    ".workbench-",
    " - kopia",
    "__coduomp",
    "docs/",
    "uodocs/",
    "/config_mp.cfg",
    "/uoconfig_mp.cfg",
    "/codkey",
    "/hunkusage.dat",
    "/games_mp.log",
    "/console_mp.log",
    "/servercache.dat",
    "/uoservercache.dat",
    "/pbcl.db",
    "/server-cache/",
)

# The retail Polish installer also drops a Linux-dedicated payload into the
# game root. None of it is used by the Windows client.
IGNORED_NON_RUNTIME = {
    "coduo_dedicated_recovered.exe",
    "etcgroup",
    "etcpasswd",
    "etcshadow",
    "forfopen.exe",
    "httpd.conf",
    "q3dirtrav.exe",
    "rconpwd.txt",
    "redhat-release",
}


def normalize(path: str) -> str:
    return path.replace("\\", "/")


def relpaths(root: str):
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(dirnames)
        for filename in sorted(filenames):
            absolute = os.path.join(dirpath, filename)
            yield normalize(os.path.relpath(absolute, root))


def configured_language(root: str) -> tuple[int, str]:
    """Return the language index the client will start with and its source.

    cl_language is archived in Main/config_mp.cfg and copied to
    uo/uoconfig_mp.cfg on first start. localization.txt names the installation
    language for installer and early-startup strings, but it does not select
    cl_language: without either config the cvar defaults to 0 (english).
    """
    for relative in ("Main/config_mp.cfg", "uo/uoconfig_mp.cfg"):
        path = os.path.join(root, *relative.split("/"))
        try:
            with open(path, "r", encoding="latin-1") as stream:
                for line in stream:
                    fields = line.split()
                    if len(fields) >= 3 and fields[0] in ("set", "seta"):
                        if fields[1].lower() == "cl_language":
                            value = fields[2].strip('"')
                            if value.isdigit():
                                index = int(value)
                                if 0 <= index < len(LANGUAGE_NAMES):
                                    return index, relative
        except OSError:
            continue

    return 0, "default"


def localized_paks(root: str, directory: str, language: str):
    prefix = f"localized_{language}_"
    try:
        names = sorted(os.listdir(os.path.join(root, directory)))
    except OSError:
        return []
    return [
        name
        for name in names
        if name.lower().startswith(prefix) and name.lower().endswith(".pk3")
    ]


def installed_uo_languages(root: str):
    """Return the languages that have a localized UO pak set installed."""
    try:
        names = os.listdir(os.path.join(root, "uo"))
    except OSError:
        return []
    installed = set()
    for name in names:
        lowered = name.lower()
        if not lowered.startswith("localized_") or not lowered.endswith(".pk3"):
            continue
        for language in LANGUAGE_NAMES:
            if lowered.startswith(f"localized_{language}_pakuo"):
                installed.add(language)
    return sorted(installed)


def compare_ignored(relative: str) -> bool:
    lowered = relative.lower()
    if lowered in IGNORED_NON_RUNTIME:
        return True
    return any(marker in lowered for marker in IGNORED_COMPARE)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Check a CoD:UO tree for the client's runtime support files."
    )
    parser.add_argument(
        "--game-root",
        required=True,
        help="directory that contains Main/ and uo/",
    )
    parser.add_argument(
        "--reference",
        help="complete installation to compare the support-file list against",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="only print problems",
    )
    arguments = parser.parse_args(argv)

    root = os.path.abspath(arguments.game_root)
    if not os.path.isdir(os.path.join(root, "Main")) or not os.path.isdir(
        os.path.join(root, "uo")
    ):
        print(
            f"error: {root} does not contain both Main/ and uo/",
            file=sys.stderr,
        )
        return 2

    failed = False
    problems = 0

    def report(kind: str, message: str) -> None:
        nonlocal problems
        problems += 1
        print(f"{kind}: {message}")

    for relative in REQUIRED:
        if not os.path.isfile(os.path.join(root, *relative.split("/"))):
            report("missing required", relative)
            failed = True

    for relative in RECOMMENDED:
        if not os.path.isfile(os.path.join(root, *relative.split("/"))):
            report("missing", relative)

    if not os.path.isfile(os.path.join(root, "mss32.dll")):
        misnamed = os.path.join(root, "mss32_dll")
        if os.path.isfile(misnamed):
            report(
                "misnamed",
                "mss32_dll must be named mss32.dll for the Miles backend",
            )

    index, source = configured_language(root)
    language = LANGUAGE_NAMES[index]
    main_paks = localized_paks(root, "Main", language)
    uo_paks = localized_paks(root, "uo", language)

    if not arguments.quiet:
        print(
            f"language: {language} ({index}) from {source}; "
            f"localized paks: Main={len(main_paks)} uo={len(uo_paks)}"
        )

    if not uo_paks:
        installed = installed_uo_languages(root)
        detail = (
            "set cl_language to an installed language"
            if installed
            else "install the matching localized UO paks"
        )
        if installed:
            detail += f" ({', '.join(installed)})"
        report(
            "missing localized assets",
            f"no localized_{language}_pakuo*.pk3 in uo/; the UO menu "
            f"backdrop (ui_mp/assets/main_back_top_mp.*) lives only in that "
            f"pak, so it renders black. Fix: {detail}",
        )
        failed = True

    if arguments.reference:
        reference = os.path.abspath(arguments.reference)
        target_prefix = ""
        try:
            relative_target = normalize(os.path.relpath(root, reference))
            if not relative_target.startswith(".."):
                target_prefix = relative_target
        except ValueError:
            pass
        present = set(relpaths(root))
        missing = [
            relative
            for relative in relpaths(reference)
            if relative.count("/") <= 1
            and not (target_prefix and (
                relative == target_prefix
                or relative.startswith(target_prefix + "/")
            ))
            and not compare_ignored(relative)
            and relative not in present
        ]
        for relative in missing:
            report("missing compared to reference", relative)
        if missing:
            failed = True

    if problems == 0:
        print("installation runtime files are complete")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
