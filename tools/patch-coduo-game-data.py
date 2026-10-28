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
    polishformat
                Drop the format insert that the Polish generic-objective hint
                adds although its caller supplies no argument. The engine keeps
                the unmatched %s in the message, so the hint shows a literal
                %s.
    percentfix  Restore the missing "s" in the format markers of the Korean and
                Taiwanese vote, timeout, and server-restart messages. A bare
                percent is not a format marker, so the value is appended after
                the sentence and the percent sign is displayed.
    shockcvar   Drop the cg_shock_viewKickFadeTime assignments from the
                shellshock parameter files. The name is not one of the 27
                cg_shock_* parameters the loader accepts, so every load warns
                that the cvar is unknown and the value is discarded.
    rconmenuload
                Restore the menus that the bundled rcon addon's replacement
                ui_mp/menus.txt drops: the UO multiplayer shoot menu, the
                PunkBuster popup menus, and the five UO game-type settings
                menus. Without them the Controls tab loses the UO weapon
                slots, the PunkBuster join and start prompts never appear, and
                the Game Type Settings tab does nothing for the UO game types.
    basbomb     Rename the base-assault defuse sound request from the model
                name MP_bomb1_defuse to the shipped alias MP_bomb_defuse.
                The misspelled request matches no alias, so defusing the bomb
                plays nothing.
    demotionsound
                Rename the mistyped multiplayer demotion alias mop_demotion to
                mp_demotion and point it at the demotion music file instead of
                the promotion one. The battle-rank script plays mp_demotion, so
                the misspelled alias is never selected and a demotion is
                silent.

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

# RE_PRESS_TO_PICKUP_GENERIC is called with no message arguments, while
# RE_PRESS_TO_PICKUP passes the objective name. The Polish generic text adds
# the same %s insert and therefore leaves the marker visible in the hint.
POLISH_FORMAT_BAD = b', aby podnie\x9c\xe6 %s cel misji"'
POLISH_FORMAT_GOOD = b', aby podnie\x9c\xe6 cel misji"'

# The Korean and Taiwanese translations store "%%" instead of "%s" in several
# patch_1_5/EXE messages. The server passes the value as a separate message
# fragment, so a bare % leaves the value appended after the sentence and prints
# the percent sign. Each tuple is (file suffix, stored line, corrected line,
# expected occurrences).
PERCENT_FIXES = (
    (
        "localizedstrings/korean/patch_1_5.str",
        b'LANG_KOREAN         "%\xb0\xa1 \xc5\xb8\xc0\xd3\xbe\xc6\xbf\xf4'
        b'\xc0\xbb \xbf\xe4\xc3\xbb\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9."',
        b'LANG_KOREAN         "%s\xb0\xa1 \xc5\xb8\xc0\xd3\xbe\xc6\xbf\xf4'
        b'\xc0\xbb \xbf\xe4\xc3\xbb\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9."',
        2,
    ),
    (
        "localizedstrings/korean/patch_1_5.str",
        b'LANG_KOREAN         "%\xb4\xc2 \xc0\xcc\xb9\xcc \xc5\xb8\xc0\xd3'
        b'\xbe\xc6\xbf\xf4\xc0\xbb \xb8\xf0\xb5\xce \xbb\xe7\xbf\xeb\xc7\xcf'
        b'\xbf\xb4\xbd\xc0\xb4\xcf\xb4\xd9."',
        b'LANG_KOREAN         "%s\xb4\xc2 \xc0\xcc\xb9\xcc \xc5\xb8\xc0\xd3'
        b'\xbe\xc6\xbf\xf4\xc0\xbb \xb8\xf0\xb5\xce \xbb\xe7\xbf\xeb\xc7\xcf'
        b'\xbf\xb4\xbd\xc0\xb4\xcf\xb4\xd9."',
        1,
    ),
    (
        "localizedstrings/korean/patch_1_5.str",
        b'LANG_KOREAN         "\xbf\xec\xc8\xa3\xc0\xfb \xc1\xf6\xc7\xa5'
        b'\xb5\xe9: %"',
        b'LANG_KOREAN         "\xbf\xec\xc8\xa3\xc0\xfb \xc1\xf6\xc7\xa5'
        b'\xb5\xe9: %s"',
        1,
    ),
    (
        "localizedstrings/korean/patch_1_5.str",
        b'LANG_KOREAN         "\xc5\xb3\xc4\xb7: %"',
        b'LANG_KOREAN         "\xc5\xb3\xc4\xb7: %s"',
        1,
    ),
    (
        "localizedstrings/korean/patch_1_5.str",
        b'LANG_KOREAN         "\xbf\xec\xc8\xa3\xc0\xfb\xc0\xce \xc8\xad'
        b'\xbf\xb0: %"',
        b'LANG_KOREAN         "\xbf\xec\xc8\xa3\xc0\xfb\xc0\xce \xc8\xad'
        b'\xbf\xb0: %s"',
        1,
    ),
    (
        "localizedstrings/korean/patch_1_5.str",
        b'LANG_KOREAN         "\xc5\xf5\xc7\xa5 %\xb8\xa6 \xc0\xa7\xc7\xd1 '
        b'\xc0\xaf\xc8\xbf\xc7\xd1 \xb3\xed\xc0\xef\xc0\xcc \xbe\xf8\xbd'
        b'\xc0\xb4\xcf\xb4\xd9"',
        b'LANG_KOREAN         "\xc5\xf5\xc7\xa5 %s\xb8\xa6 \xc0\xa7\xc7\xd1 '
        b'\xc0\xaf\xc8\xbf\xc7\xd1 \xb3\xed\xc0\xef\xc0\xcc \xbe\xf8\xbd'
        b'\xc0\xb4\xcf\xb4\xd9"',
        1,
    ),
    (
        "localizedstrings/korean/patch_1_5.str",
        b'LANG_KOREAN         "%\xb0\xa1 \xc5\xb8\xc0\xd3\xc0\xce\xc0\xbb '
        b'\xba\xd2\xb7\xb6\xbd\xc0\xb4\xcf\xb4\xd9."',
        b'LANG_KOREAN         "%s\xb0\xa1 \xc5\xb8\xc0\xd3\xc0\xce\xc0\xbb '
        b'\xba\xd2\xb7\xb6\xbd\xc0\xb4\xcf\xb4\xd9."',
        1,
    ),
    (
        "localizedstrings/korean/patch_1_5.str",
        b'LANG_KOREAN         "%\xb0\xa1 \xbb\xe7\xbf\xeb \xb0\xa1\xb4\xc9'
        b'\xc7\xd1 \xc5\xb8\xc0\xd3\xbe\xc6\xbf\xf4\xc0\xbb \xb8\xf0\xb5\xce '
        b'\xbb\xe7\xbf\xeb\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9."',
        b'LANG_KOREAN         "%s\xb0\xa1 \xbb\xe7\xbf\xeb \xb0\xa1\xb4\xc9'
        b'\xc7\xd1 \xc5\xb8\xc0\xd3\xbe\xc6\xbf\xf4\xc0\xbb \xb8\xf0\xb5\xce '
        b'\xbb\xe7\xbf\xeb\xc7\xdf\xbd\xc0\xb4\xcf\xb4\xd9."',
        1,
    ),
    (
        "localizedstrings/taiwanese/exe.str",
        b'LANG_TAIWANESE      "\xa5\xd1\xa9\xf3% \xa6]\xaf\xc0\xa1A\xad\xab'
        b'\xb7s\xb1\xd2\xb0\xca\xa6\xf8\xaaA\xbe\xb9"',
        b'LANG_TAIWANESE      "\xa5\xd1\xa9\xf3%s \xa6]\xaf\xc0\xa1A\xad\xab'
        b'\xb7s\xb1\xd2\xb0\xca\xa6\xf8\xaaA\xbe\xb9"',
        1,
    ),
)

# CG_ShellShockLoad parses a .shock file through Com_LoadCvarsFromBuffer,
# which matches every name against the 27-entry cg_shockParamNames table. A
# name outside the table is skipped with an "unknown cvar" warning and never
# takes effect. cg_shock_viewKickFadeTime is absent from the table (the view
# kick only has cg_shock_viewKickPeriod and cg_shock_viewKickRadius), so the
# shipped assignment is dead data. Each tuple is (file suffix, lines to remove);
# a suffix may list a commented mirror of the default block as well.
SHOCK_FIXES = (
    ("scripts/default_mp.shock", (b'cg_shock_viewKickFadeTime "3"\r\n',)),
    ("scripts/death.shock", (b'cg_shock_viewKickFadeTime "0"\r\n',)),
    ("scripts/default.shock", (b'cg_shock_viewKickFadeTime "3"\r\n',)),
    (
        "scripts/default_nomusic.shock",
        (b'cg_shock_viewKickFadeTime "3"\r\n',),
    ),
    (
        "scripts/groggy.shock",
        (
            b'//cg_shock_viewKickFadeTime "3"\r\n',
            b'cg_shock_viewKickFadeTime "3"\r\n',
        ),
    ),
    ("scripts/pain.shock", (b'cg_shock_viewKickFadeTime "0.3"\r\n',)),
    ("scripts/stop.shock", (b'cg_shock_viewKickFadeTime "0"\r\n',)),
)

# The bundled rcon addon ships a complete replacement of ui_mp/menus.txt. It
# was written against an older menu list, so it enables the single-player shoot
# menu instead of the UO multiplayer one and drops three stock groups: the UO
# multiplayer shoot menu (which adds the smoke grenade, satchel, and binocular
# weapon slots), the PunkBuster popup menus used by the join and start paths,
# and five UO game-type settings menus. Every menu named below is present in
# the retail tree; only the addon's loader list omits it.
RCON_LOAD_SHOOT_SP = b'\tloadMenu { "ui/options_shoot.menu" }\r\n'
RCON_LOAD_SHOOT_MP = b'\tloadMenu { "ui_mp/options_shoot.menu" }\r\n'
RCON_LOAD_SHOOT_SP_OFF = b'//\tloadMenu { "ui/options_shoot.menu" }\r\n'
RCON_LOAD_MULTI = b'\tloadMenu { "ui_mp/options_multi.menu" }\r\n'
RCON_LOAD_JOIN = b' \tloadMenu { "ui_mp/joinserver.menu" }\r\n'
RCON_LOAD_PB = b'\tloadMenu { "ui_mp/pb_popmenus.menu" }\r\n'
RCON_LOAD_BEL = b'\tloadMenu { "ui_mp/settings_bel.menu" }\t//TEMP\r\n'
RCON_LOAD_SETTINGS = (
    b'\tloadMenu { "ui_mp/settings_hq.menu" }\r\n'
    b'\tloadMenu { "ui_mp/settings_ttdm.menu" }\r\n'
    b'\tloadMenu { "ui_mp/settings_dom.menu" }\r\n'
    b'\tloadMenu { "ui_mp/settings_bas.menu" }\r\n'
    b'\tloadMenu { "ui_mp/settings_ctf.menu" }\r\n'
)
RCON_LOAD_SETTINGS_MENUS = (
    b'\tloadMenu { "ui_mp/settings_hq.menu" }\r\n',
    b'\tloadMenu { "ui_mp/settings_ttdm.menu" }\r\n',
    b'\tloadMenu { "ui_mp/settings_dom.menu" }\r\n',
    b'\tloadMenu { "ui_mp/settings_bas.menu" }\r\n',
    b'\tloadMenu { "ui_mp/settings_ctf.menu" }\r\n',
)

# Base assault builds its bomb objective from two models, mp_bomb1 and
# mp_bomb1_defuse, and plays the defuse sound with a name copied from the
# model: MP_bomb1_defuse. The sound alias bank defines MP_bomb_defuse (and
# MP_bomb_plant), and every search-and-destroy script plays MP_bomb_defuse.
# A request for a name that no CSV defines selects no alias, so the client
# returns without playing anything and without printing a warning.
BAS_BOMB_BAD = b'other playsound("MP_bomb1_defuse");'
BAS_BOMB_GOOD = b'other playsound("MP_bomb_defuse");'

# The battle-rank script plays the multiplayer demotion alias by name:
#     player playLocalSound("mp_demotion");
# The alias table spells the row mop_demotion, and that row also points at the
# promotion music. The same table already stores the demotion music under the
# ge_demotion and uk_demotion rows, and sound/music/mp_demotion.mp3 ships in
# uo/pakuo02.pk3. A name no row defines selects no alias, so the client returns
# without playing anything and without printing a warning.
DEMOTION_BAD = b"mop_demotion,,music/mp_promotion.mp3,1,"
DEMOTION_GOOD = b"mp_demotion,,music/mp_demotion.mp3,1,"

FIX_NAMES = (
    "retrieval",
    "polish",
    "rconmenu",
    "polishquotes",
    "polishformat",
    "percentfix",
    "shockcvar",
    "rconmenuload",
    "basbomb",
    "demotionsound",
)

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


def plan_polishformat(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir():
            continue
        if not info.filename.lower().endswith(
            "localizedstrings/polish/re.str"
        ):
            continue
        data = archive.read(info)
        bad_count = data.count(POLISH_FORMAT_BAD)
        if bad_count == 0:
            continue
        if bad_count != 1:
            raise ValueError(
                f"{label}: {info.filename}: expected one unmatched Polish "
                f"format insert, found {bad_count}"
            )
        replacements[info.filename] = data.replace(
            POLISH_FORMAT_BAD, POLISH_FORMAT_GOOD, 1
        )
    return replacements


def plan_percentfix(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir():
            continue
        lower = info.filename.lower()
        group = [
            (old, new, expected)
            for suffix, old, new, expected in PERCENT_FIXES
            if lower.endswith(suffix)
        ]
        if not group:
            continue
        data = archive.read(info)
        counts = [data.count(old) for old, _, _ in group]
        if all(count == 0 for count in counts):
            continue
        expected = [item[2] for item in group]
        if counts != expected:
            raise ValueError(
                f"{label}: {info.filename}: expected {expected} malformed "
                f"format markers, found {counts}"
            )
        for old, new, _ in group:
            data = data.replace(old, new)
        replacements[info.filename] = data
    return replacements


def plan_shock(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir():
            continue
        lower = info.filename.lower()
        group = [
            lines for suffix, lines in SHOCK_FIXES if lower.endswith(suffix)
        ]
        if not group:
            continue
        if len(group) != 1:
            raise ValueError(
                f"{label}: {info.filename}: matches several .shock entries"
            )
        data = archive.read(info)
        if not any(line in data for line in group[0]):
            continue
        for line in group[0]:
            count = data.count(line)
            if count == 0:
                continue
            if count != 1:
                raise ValueError(
                    f"{label}: {info.filename}: expected one occurrence of "
                    f"{line!r}, found {count}"
                )
            data = data.replace(line, b"", 1)
        if b"cg_shock_viewKickFadeTime" in data:
            raise ValueError(
                f"{label}: {info.filename}: unexpected "
                f"cg_shock_viewKickFadeTime line remains"
            )
        replacements[info.filename] = data
    return replacements


def plan_rconmenuload(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir() or not info.filename.lower().endswith(
            "ui_mp/menus.txt"
        ):
            continue
        data = archive.read(info)
        # Only the bundled addon replaces the whole list. The stock files and
        # the CoD1 patch already load every menu restored here.
        if b'"ui_mp/scriptmenus/rconmenu.menu"' not in data:
            continue

        required = (
            RCON_LOAD_PB,
            RCON_LOAD_SHOOT_MP,
            RCON_LOAD_SHOOT_SP_OFF,
        ) + RCON_LOAD_SETTINGS_MENUS
        if all(data.count(token) == 1 for token in required):
            continue

        anchors = (
            RCON_LOAD_SHOOT_SP,
            RCON_LOAD_MULTI,
            RCON_LOAD_JOIN,
            RCON_LOAD_BEL,
        )
        counts = [data.count(anchor) for anchor in anchors]
        if counts != [1, 1, 1, 1]:
            raise ValueError(
                f"{label}: {info.filename}: expected one copy of each rcon "
                f"menu anchor, found {counts}"
            )
        if any(data.count(token) != 0 for token in required):
            raise ValueError(
                f"{label}: {info.filename}: rcon menu list is partially "
                f"patched"
            )

        data = data.replace(
            RCON_LOAD_SHOOT_SP, RCON_LOAD_SHOOT_SP_OFF, 1
        )
        data = data.replace(
            RCON_LOAD_MULTI, RCON_LOAD_MULTI + RCON_LOAD_SHOOT_MP, 1
        )
        data = data.replace(
            RCON_LOAD_JOIN, RCON_LOAD_JOIN + RCON_LOAD_PB, 1
        )
        data = data.replace(
            RCON_LOAD_BEL, RCON_LOAD_BEL + RCON_LOAD_SETTINGS, 1
        )
        if not all(data.count(token) == 1 for token in required):
            raise ValueError(
                f"{label}: {info.filename}: restored menu list does not "
                f"contain every required entry"
            )
        replacements[info.filename] = data
    return replacements


def plan_basbomb(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir() or not info.filename.lower().endswith(
            "gametypes/bas.gsc"
        ):
            continue
        data = archive.read(info)
        bad_count = data.count(BAS_BOMB_BAD)
        good_count = data.count(BAS_BOMB_GOOD)
        if bad_count == 0 and good_count == 1:
            continue
        if bad_count != 1 or good_count != 0:
            raise ValueError(
                f"{label}: {info.filename}: expected exactly one base-assault "
                f"defuse sound request, found {bad_count} misspelled and "
                f"{good_count} corrected"
            )
        replacements[info.filename] = data.replace(
            BAS_BOMB_BAD, BAS_BOMB_GOOD, 1
        )
    return replacements


def plan_demotionsound(archive: zipfile.ZipFile, label: str) -> dict[str, bytes]:
    replacements = {}
    for info in archive.infolist():
        if info.is_dir() or not info.filename.lower().endswith(
            "soundaliases/generic_mp_sound.csv"
        ):
            continue
        data = archive.read(info)
        bad_count = data.count(DEMOTION_BAD)
        good_count = data.count(DEMOTION_GOOD)
        if bad_count == 0 and good_count == 1:
            continue
        if bad_count != 1 or good_count != 0:
            raise ValueError(
                f"{label}: {info.filename}: expected exactly one multiplayer "
                f"demotion alias row, found {bad_count} mistyped and "
                f"{good_count} corrected"
            )
        replacements[info.filename] = data.replace(
            DEMOTION_BAD, DEMOTION_GOOD, 1
        )
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
    "polishformat": (
        "Polish generic-objective format fix",
        plan_polishformat,
    ),
    "percentfix": (
        "Korean and Taiwanese format marker fix",
        plan_percentfix,
    ),
    "shockcvar": (
        "dead shellshock view-kick fade entry fix",
        plan_shock,
    ),
    "rconmenuload": (
        "rcon addon menu list fix",
        plan_rconmenuload,
    ),
    "basbomb": (
        "base-assault bomb defuse sound fix",
        plan_basbomb,
    ),
    "demotionsound": (
        "multiplayer demotion sound alias fix",
        plan_demotionsound,
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
