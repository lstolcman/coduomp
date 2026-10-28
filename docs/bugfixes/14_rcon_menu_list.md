# The bundled rcon addon replaces the multiplayer menu list and drops stock menus

## Symptom

The Polish 1.51b installation ships `zzz_rconmenu.pk3`, a server-control
addon. The archive contains a complete `ui_mp/menus.txt`, and that file does
not merely add the addon's own menus: it also removes several menus that the
stock list loads. Three separate user-visible regressions follow.

1. **Game Type Settings does nothing for four UO game types.** On *Create
   Server*, the game type selector offers Base Assault, Capture the Flag,
   Domination, Headquarters, and the older modes. Pressing **Game Type
   Settings** opens a panel for Deathmatch, Team Deathmatch, Search & Destroy,
   Retrieval, and Behind Enemy Lines, but for Base Assault, Capture the Flag,
   Domination, and Headquarters the click produces no panel at all and no
   message. The tab is simply dead.
2. **The multiplayer Controls tab loses the UO weapon slots.** The *Controls
   → Shoot* page shows Fire, Aim, Melee, Reload, weapon slots 1 and 2, pistol,
   and grenade, but no Smoke Grenade, Satchel, or Binoculars bind rows, even
   though the UO game binds and uses all three.
3. **The PunkBuster prompts never appear.** When the player joins a
   PunkBuster-protected server without `cl_punkbuster` set, or presses
   **Start Server** in that state, the client is supposed to show a popup that
   explains the situation and offers Yes/No. Instead the button appears to do
   nothing: no popup, no connect, no error.

All three are silent. The menu loader does not report a missing menu, so the
console log contains no warning and the only evidence is the absence of the
panel or popup.

## Investigation

### The addon supplies the whole menu list

The UI loads exactly one menu list file, named by `ui_menuFiles`. At startup
the cvar is `ui_mp/menus.txt`:

```
      cvar set ui_menuFiles ui_mp/menus.txt
```

`UI_Load` (`src/client/ui/menu/ui_load.c:18-31`) reads that one name and calls
`UI_LoadMenus(menuFile, qtrue, UI_MENU_LOAD_ALL)`. It does not merge the
`loadMenu` blocks of several files; only the highest-priority archive that
provides `ui_mp/menus.txt` is parsed. The runtime search path confirms which
archive that is:

```
Current search path:
C:\...\_zzzz\uo\zzz_rconmenu.pk3 (31 files)
C:\...\_zzzz\uo\pakuo07.pk3 (157 files)
...
C:\...\_zzzz\uo\pakuo03.pk3 (2275 files)
...
C:\...\_zzzz\main\pakb.pk3 (60 files)
```

`uo/zzz_rconmenu.pk3` is first in the search path, ahead of `uo/pakuo03.pk3`
and `Main/pakb.pk3`, so its 60-line list is the effective one. The stock list
in `uo/pakuo03.pk3` has 61 lines and loads every menu discussed below.

The addon is a retail component: it exists in the Polish reference tree
`_CoD1_UO_1_51b polish` in both `uo/` and `Main/` with identical bytes
(SHA-256 `6af8899a0a71c62e9d0eec0647cc84d5512c40a6647a32617a174e4add1b1b3e`),
and the English reference tree does not ship it. It was presumably written
against an older menu list and was never reconciled with the UO 1.51 list.

### Game Type Settings

`ui_mp/createserver.menu` (in `uo/pakuo03.pk3`) binds the tab button to the
game type name:

```
cvar			"ui_netGametypeName"
...
openForGameType "settings_%s";
```

The cvar is written by the UI when the game type selection changes.
`UI_GetGameTypesList` (`src/client/ui/state/ui_get_game_types_list.c:28-57`)
builds the table from the scripts in `maps/mp/gametypes/`, so `gameType` is
the script basename: `bas`, `bel`, `ctf`, `dm`, `dom`, `hq`, `re`, `sd`,
`tdm`. `UI_SelectCurrentGameType` and the game type key handler store that
string in `ui_netGameTypeName`, and cvar lookup is case-insensitive, so the
menu's `ui_netGametypeName` reads it.

`Script_OpenForGameType` (`src/client/menu/ui_script_menu_navigation.c:56-77`)
formats the name and resolves it:

```
menu = Menus_FindByName(
    client_menu_compat_format_game_type(format, gameType));
if (menu != NULL) {
    Menus_Open(menu);
}
```

A name that no loaded menu matches is ignored without a message, which is why
the button appears to do nothing.

The ten settings panels are shipped as separate files:

```
uo/pakuo03.pk3::ui_mp/settings_dm.menu    name "settings_dm"
uo/pakuo03.pk3::ui_mp/settings_tdm.menu   name "settings_tdm"
uo/pakuo03.pk3::ui_mp/settings_sd.menu    name "settings_sd"
uo/pakuo03.pk3::ui_mp/settings_re.menu    name "settings_re"
uo/pakuo03.pk3::ui_mp/settings_bel.menu   name "settings_bel"
uo/pakuo03.pk3::ui_mp/settings_hq.menu    name "settings_hq"
uo/pakuo03.pk3::ui_mp/settings_ttdm.menu  name "settings_ttdm"
uo/pakuo03.pk3::ui_mp/settings_dom.menu   name "settings_dom"
uo/pakuo03.pk3::ui_mp/settings_bas.menu   name "settings_bas"
uo/pakuo03.pk3::ui_mp/settings_ctf.menu   name "settings_ctf"
```

The install ships no `ttdm.gsc`, so the selector never offers `ttdm` and that
panel is unreachable; the stock list still loads it. The other nine names
match a shipped game type script.

They are not included by any other menu file; the only thing that loads them
is the `loadMenu` list. The stock list loads all ten. The addon's list loads
the first five and then replaces the UO five with five names that exist in no
archive:

```
	loadMenu { "ui_mp/settings_dm.menu" }	//TEMP
	loadMenu { "ui_mp/settings_tdm.menu" }	//TEMP
	loadMenu { "ui_mp/settings_sd.menu" }	//TEMP
	loadMenu { "ui_mp/settings_re.menu" }	//TEMP
	loadMenu { "ui_mp/settings_bel.menu" }	//TEMP

	loadMenu { "ui_mp/settings_obj.menu" }	//ham
	loadMenu { "ui_mp/settings_rb.menu" }	//ham
	loadMenu { "ui_mp/settings_rsd.menu" }	//Boco
	loadMenu { "ui_mp/settings_rsd_bomb.menu" }	//Boco
	loadMenu { "ui_mp/settings_rsd_guns.menu" }	//Boco
```

`settings_obj`, `settings_rb`, and the three `settings_rsd*` names are inert:
they match nothing in either game tree, so they load nothing and cost
nothing. The five missing UO names are the defect.

### Multiplayer shoot menu

The stock UO list comments out the single-player file and loads the
multiplayer variant:

```
//	loadMenu { "ui/options_shoot.menu" }
...
	loadMenu { "ui_mp/options_shoot.menu" }
```

The addon does the reverse: it enables `ui/options_shoot.menu` on line 9 and
deletes `ui_mp/options_shoot.menu`. Both files define the same menu name,
`options_shoot`, and `ui/options.menu` opens that name from the Controls tab
(`open options_shoot;`), so only the loaded definition is shown.

The two files differ in exactly the weapon rows that UO added. The
multiplayer variant (`uo/localized_polish_pakuo00.pk3::ui_mp/options_shoot.menu`)
contains these three binds, which the single-player variant does not:

```
	text		"@GMI_MENU_SELECT_SMOKE"       cvar "weaponslot smokegrenade"
	text		"@GMI_MENU_SELECT_SATCHEL"     cvar "weaponslot satchel"
	text		"@GMI_MENU_SELECT_BINOCULARS"  cvar "weaponslot binocular"
```

That is the observed loss of rows, and it restores the bind rows that the UO
gameplay code expects the player to be able to edit.

### PunkBuster popups

`ui_mp/pb_popmenus.menu` (in `Main/paka.pk3`) defines `joinpb_popmenu` and
`startpb_popmenu`. Two UI script paths open them by name:

- `JoinServer` (`src/client/ui/state/ui_run_menu_script.c:280-284`) opens
  `joinpb_popmenu` when the selected server uses PunkBuster and the client
  does not.
- `StartServer` (`src/client/ui/state/ui_run_menu_script.c:44-53`) opens
  `startpb_popmenu` under the same condition before starting a local server.

`Menus_OpenByName` ignores an unknown name, so with the menu list missing the
`loadMenu` line the two paths return without opening anything and without
printing an error. The stock list loads the file (`loadMenu {
"ui_mp/pb_popmenus.menu" }`); the addon deletes that line.

### What is not a defect

- The five `settings_obj`/`settings_rb`/`settings_rsd*` entries are inert
  addon leftovers. They do not break parsing and do not shadow a real menu, so
  the fix leaves them in place rather than editing unrelated addon content.
- `ui_mp/scriptmenus/admin_main.menu` is dropped by the addon but does not
  exist in any archive, so no behavior changes.
- The addon's own `rconmenu.menu` and `privatepassword.menu` loads are intact
  and remain loaded after the fix.

## Fix

`tools/patch-coduo-game-data.py` gains a `rconmenuload` fix. It only touches an
`ui_mp/menus.txt` entry that contains the addon marker
`"ui_mp/scriptmenus/rconmenu.menu"`, so the stock lists in `pakuo03.pk3`,
`pakb.pk3`, and `pak0.pk3` are never rewritten. In that file it makes three
insertions:

1. comment out `ui/options_shoot.menu` and add
   `ui_mp/options_shoot.menu` after `ui_mp/options_multi.menu`;
2. add `ui_mp/pb_popmenus.menu` after `ui_mp/joinserver.menu`;
3. add the five missing UO settings panels after `settings_bel.menu`.

The tool requires exactly one copy of each anchor and no partially applied
state, then verifies that every restored line is present exactly once before
the archive replaces the original. A second run reports that no archive needs
a change, which makes the fix idempotent. `.bugfix-backup` copies are kept
when `--backup` is used:

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix rconmenuload
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix rconmenuload --apply --backup
```

## Validation

- Dry run reported exactly four archives: `uo/` and `Main/` in the installation
  and the same two in `_zzzz`.
- `--apply --backup` changed the menu list from 60 to 67 lines in all four.
  The archive hash
  `398a2cfe6bb1f7881f1772d80a4355b7962df630d380231b8eb085eca4759743`
  became
  `05175a9d96b47515b9a162dc5c647cf18595d67dfd903d8952ccaa26f32693af`.
- Re-running the dry run prints `no archive needed a change`.
- The patched list contains one active `ui_mp/options_shoot.menu` load, one
  commented-out `ui/options_shoot.menu` load, one `ui_mp/pb_popmenus.menu`
  load, and one load each for `settings_hq`, `settings_ttdm`, `settings_dom`,
  `settings_bas`, and `settings_ctf`, while the addon's own menus remain
  loaded.
- Ran the `_zzzz` client with `+set g_gametype dom +map mp_foy`,
  `developer 1`, and `logfile 1` for 40 seconds. The archive still loads with
  31 files and the log contains no menu parse error, no missing-menu message,
  and no new warning compared with the previous run.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/14_rcon_menu_list.md`
