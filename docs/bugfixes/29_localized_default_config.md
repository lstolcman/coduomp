# Bug 29 - Non-English installs fail before reading default_mp.cfg

## Symptom

A dedicated server on an original non-English game-data tree failed during
filesystem startup. The Polish tree is representative:

```text
CoD:UO Recovered Dedicated Engine 1.51 (linux-x86_64) ...
----- FS_Startup -----
Current language: english
Current search path:
...
52326 files in pk3 files
Hunk_Clear: reset the hunk ok
Sys_Error: Couldn't load default_mp.cfg.  Make sure Call of Duty is run from the correct folder.
```

The non-English tree keeps `default_mp.cfg` only inside its localized paks:

```text
localized_polish_pak0.pk3      default_mp.cfg
localized_polish_pak2.pk3      default_mp.cfg
localized_polish_pak5.pk3      default_mp.cfg
localized_polish_pakuo00.pk3   default_mp.cfg
localized_polish_pakuo01.pk3   default_mp.cfg
```

The same layout is used by every supported non-English language; the file name
carries the language selected from the fourteen-entry table.

## Root cause

The engine uses the Windows filesystem language policy: `FS_UseSearchPath()`
accepts a localized search path only when its language matches
`cl_language->integer`. The dedicated server creates `cl_language` with the
default value `0` (English). No command-line or archived client setting supplies
another language, so the localized paks stay hidden and `default_mp.cfg` is
reported missing before any server config is executed.

## Fix

`src/filesystem/filesystem_startup.c` now loads the default configuration
through a helper that keeps the Windows policy when a language was explicitly
selected:

- `+set cl_language <index>` still wins, including an explicit English choice.
- Without a command-line language, the helper tries the installed localized
  languages in the same descending order as the localized search paths and
  keeps the first language whose paks provide `default_mp.cfg`.
- If no localized language provides the file, the original
  `Couldn't load default_mp.cfg` error is preserved.
- The filesystem restart path keeps its existing default-config check, so a
  language selected later by a config is not overridden.
- The localized-asset cvar spelling is corrected from `fs_ignoreLozalized` to
  `fs_ignoreLocalized`, matching the variable and the documented name.

## Localized assets and pure PK3 checks

The pure-server lists never include localized paks:

- `FS_LoadedPakNames()`, `FS_LoadedPakChecksums()` and
  `FS_LoadedPakPureChecksums()` skip every `search->localized` entry, so
  `sv_pakNames` contains only the shared `main`/`uo` paks.
- `FS_FileIsInPAK()` bypasses the pure-server filter for localized packs.
- `FS_ComparePaks()` skips files classified by `FS_idPak()`, which includes
  localized paks.

Verified with the original English and Polish trees, `sv_pure 1`, and a loaded
map. Both servers reported:

```text
"sv_pure" is:"1"
"sv_pakNames" is:"uomappack00 pakuo07 pakuo06 pakuo05 pakuo04 pakuo03
pakuo02 pakuo01 pakuo00 pakb paka pak9 pak8 pak6 pak5 pak4 pak3 pak2 pak1 pak0"
```

No `localized_*` pak appears in the list on either tree. A client therefore does
not need to provide the server's localized paks to pass the pure check; the
localized files remain server-side presentation/data assets.

## Validation

- Original Polish tree, no `cl_language`: selected Polish and executed
  `default_mp.cfg`.
- Original English tree, no `cl_language`: started with English assets.
- Original Polish tree with `+set cl_language 0`: still failed with the original
  missing-config error, confirming an explicit language wins.
- Original Polish tree with `+set cl_language 7`: started with Polish assets.
- `sv_pure 1` on both trees: the loaded pak list above.
- Linux server and the MinGW Windows server target build with the same shared
  filesystem source.
