# A copied installation loses its menu backdrop and runtime support files

## Symptom

A tree assembled by copying only `Main/` and `uo/` (the pk3 archives and the
module DLLs) plus `CoDUOMP.exe` starts, loads maps, and plays, but the main
menu is visibly malformed:

- the upper two thirds of the menu are black instead of showing the
  Call of Duty: United Offensive artwork;
- only a strip of the background image appears at the bottom, tiled
  horizontally;
- the menus use the English labels even though the installed archives are the
  Polish localized set.

The bottom strip is the `main_back_bottom` item; the missing area is
`main_back_top`.

## Investigation

`ui_mp/main.menu` for the multiplayer client includes
`ui_mp/menu_background.menu`, which defines two decoration items:

```
itemDef { name main_back_top    rect 0   0 640 320
          background "ui_mp/assets/main_back_top_mp.dds" }
itemDef { name main_back_bottom rect 0 320 640 160
          background "ui_mp/assets/main_back_bottom_mp.dds" }
```

The two halves are shipped in different archives:

| Asset | Archive |
| --- | --- |
| `ui_mp/assets/main_back_top_mp.dds` | `uo/localized_polish_pakuo00.pk3` |
| `ui_mp/assets/main_back_bottom_mp.dds` | `uo/pakuo03.pk3` |

The top half exists only in the language-specific pak, not in the base
`pakuo03.pk3`, and the English localized UO paks are not part of this
installation. Localized archives are mounted only for the language selected by
`cl_language` (0 english ... 7 polish in the original table at `0x005ca678`).
With the default `cl_language 0` the engine mounts `localized_english_pak*`
from `Main/` (those archives are present) but no `localized_english_pakuo*`
from `uo/`, so `main_back_top_mp.dds` cannot be found and the item draws
nothing.

`cl_language` is an archived cvar. Its value lives in `Main/config_mp.cfg`
and is copied to `uo/uoconfig_mp.cfg` on first start. Both files were absent
from the copied tree, so the cvar fell back to its default of `0`. The root
`localization.txt`, which names `polish`, is not consulted for this decision:
`Sys_InitLocalization` only feeds installer and early-startup strings.
Verified by running the incomplete tree three ways:

| Tree state | Menu |
| --- | --- |
| no config, no `localization.txt` | black top half, English labels |
| `localization.txt` added | black top half, English labels |
| `+set cl_language 7` | complete Polish menu |
| restored `Main/config_mp.cfg` | complete Polish menu |

The other missing files were listed while reproducing the report:

| File | Effect when absent |
| --- | --- |
| `mss32.dll` (present but named `mss32_dll`) | Miles backend unavailable. The recovered client builds with the Miniaudio/OpenAL backends and prints `Miles audio is unavailable; using Miniaudio`, so sound still works, but the retail Miles path and `miles/*` codecs are not used. |
| `miles/` | Miles codec and provider DLLs; only reachable through `mss32.dll`. |
| `binkw32.dll` | Not used by the recovered client: `cinematic.c` decodes RoQ video itself. The retail executable needed it for movies. |
| `EaxMan.dll` | EAX is optional; `InitEAXManager` opens the DLL dynamically and stays disabled when it is absent. |
| `validatekey.dll` | Not referenced by the recovered client, which performs CD-key authorization over the network. Retail legacy payload. |
| `Main/config_mp.cfg` | Archived user settings, including `cl_language`; absent means defaults, which is what caused the black menu here. |
| `uo/uoconfig_mp.cfg` | UO archived settings and binds; recreated from defaults. |
| `Main/cgame_mp_x86.dll`, `Main/game_mp_x86.dll`, `Main/ui_mp_x86.dll` | Base-game modules. The UO client loads `uo/uo_*_mp_x86.dll`, so UO still runs; these are needed by the base-game client. |
| `cgamex86.dll`, `gamex86.dll`, `uix86.dll`, `uo_cgamex86.dll`, `uo_gamex86.dll`, `uo_uix86.dll` | Single-player and base-game modules; not loaded by `CoDUOMP.exe`. |
| `cod.bmp`, `coduo.bmp` | Launcher and installer artwork; no runtime effect. |
| `pbcl.db`, `servercache.dat`, `uoservercache.dat` | Client caches; recreated. |
| `uo/version.inf` | UO version stamp read by installers and version tooling. |
| `Docs/`, `UODocs/` | Manuals and readmes. |
| `etcgroup`, `etcpasswd`, `etcshadow`, `forfopen.exe`, `httpd.conf`, `q3dirtrav.exe`, `rconpwd.txt`, `redhat-release` | Linux dedicated-server payload that the retail Polish installer copied into the game root; unused by the Windows client. |

The files that must be beside the executables are therefore the root-level
support files and the two configuration files; `Main/` and `uo/` alone are not
a complete client.

## Fix

The runtime files themselves cannot be generated from the source tree; they
come from the retail installation. The fix is to restore the support files:

1. Copy the root-level files from a complete installation:
   `localization.txt`, `version.inf`, `mss32.dll`, `miles/`, `binkw32.dll`,
   `EaxMan.dll`, `validatekey.dll`, `cod.bmp`, `coduo.bmp`, `pbcl.db`,
   `servercache.dat`, and the retail module DLLs
   (`cgamex86.dll`, `gamex86.dll`, `uix86.dll`, `uo_cgamex86.dll`,
   `uo_gamex86.dll`, `uo_uix86.dll`).
2. Copy `Main/config_mp.cfg`, `Main/cgame_mp_x86.dll`,
   `Main/game_mp_x86.dll`, `Main/ui_mp_x86.dll`, `uo/uoconfig_mp.cfg`, and
   `uo/version.inf`.
3. Run the checker to confirm the result:

   ```
   tools/check-coduo-install.py --game-root /path/to/copied
   tools/check-coduo-install.py --game-root /path/to/copied \
       --reference /path/to/complete
   ```

If the configuration must stay absent, the minimum repair is to select the
language whose localized UO pak is installed, for example
`+set cl_language 7` on the command line; the archived value is then written
on exit. `localization.txt` alone is not sufficient.

`tools/check-coduo-install.py` performs those checks without writing anything.
It reports required and recommended files, detects the misnamed `mss32_dll`,
derives the effective language from the archived configuration exactly as the
client does, and fails when that language has no `localized_<language>_pakuo*`
archive. With `--reference` it also lists top-level support files that exist in
a complete tree but not in the copy, ignoring logs, caches, user backups,
documentation, and the Linux-dedicated payload.

## Validation

- Reproduced the black top half with the incomplete tree and captured it.
- Adding only `localization.txt` did not change the menu.
- Adding only `localization.txt` plus `Main/config_mp.cfg` did not change it
  until the configuration selected Polish (`cl_language 7` in the shipped
  file); the same result was obtained with `+set cl_language 7` alone.
- After restoring the listed files, the menu matches the complete installation,
  including the Polish labels and the full backdrop.
- `tools/check-coduo-install.py` reports
  `language: english (0) from default; localized paks: Main=3 uo=0` and the
  missing-assets error for the original tree, and
  `language: polish (7) from Main/config_mp.cfg; localized paks: Main=5 uo=3`
  followed by `installation runtime files are complete` for the repaired tree.
- `--reference` reports no missing support file after the repair.
- `make policy-check` passes.

## Notes

The missing top half is a data-packaging consequence, not a renderer defect.
The renderer behaves the same way for any missing menu shader image: the item
draws nothing and the item underneath it remains visible. The engine already
prints shader warnings for world textures; menu item shaders are registered
through the UI module and do not produce a comparable failure line, which is
why the copied tree looked simply "broken" rather than reporting an error.

## Files

- `tools/check-coduo-install.py`
- `docs/bugfixes/07_incomplete_client_install.md`
