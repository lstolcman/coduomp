# Shellshock files load an assignment the client does not recognize

## Symptom

Every map load prints a shellshock warning into the console, once per
shellshock file the game reads:

```
^3WARNING: unknown cvar 'cg_shock_viewKickFadeTime' in file 'scripts/default_mp.shock'
```

The line is harmless for gameplay, but it is not expected output: the shipped
data assigns a value that the client can never apply, and the warning is shown
to every player with the console open. With `logfile 1` it is also written to
`console_mp.log` on each start.

## Investigation

Shellshock parameter files are loaded by `CG_ShellShockLoad`
(`src/client/cgame/effects/cg_shellshockload.c`). It opens
`scripts/<name>.shock`, reads the whole file, and hands the text to
`Com_LoadCvarsFromBuffer` through `CG_COM_LOAD_CVARS_FROM_BUFFER`, together with
the 27-entry table `cg_shockParamNames`
(`src/client/cgame/globals.c`):

```
cg_shock_screenBlendTime, cg_shock_screenBlendFadeTime,
cg_shock_viewKickPeriod,  cg_shock_viewKickRadius,
cg_shock_sound, ... cg_shock_mouse_fadeTime
```

`Com_LoadCvarsFromBuffer` (`src/qcommon/q_cvar.c`) compares each parsed name
against that table with `Q_stricmp`. A name that matches is applied with
`Cvar_Set2`; a name that does not match is skipped and reported:

```c
if (index == cvarCount) {
    Com_Printf(
        "^3WARNING: unknown cvar '%s' in file '%s'\n",
        name, fileName);
    Com_SkipRestOfLine(&buffer);
    continue;
}
```

`cg_shockParamNames` has no `cg_shock_viewKickFadeTime` entry, and the
shellshock parameter block built by `CG_SetShellShockParams`
(`src/client/cgame/effects/cg_setshellshockparams.c`) has no matching field
either. The view kick is described only by two values: `period` becomes the
blur rate (`0.001f / max(period, 0.001f)`) and `radius` becomes the blur scale.
There is no fade time to feed, so the shipped assignment was silently
discarded while producing the warning.

The entry is present in all seven shellshock parameter files in both `Main/`
and `uo/`:

| Archive | Entry | Value |
| --- | --- | --- |
| `uo/pakuo01.pk3` | `scripts/default_mp.shock` | `3` |
| `Main/pak4.pk3` | `scripts/death.shock` | `0` |
| `Main/pak4.pk3` | `scripts/default.shock` | `3` |
| `Main/pak4.pk3` | `scripts/default_nomusic.shock` | `3` |
| `Main/pak4.pk3` | `scripts/groggy.shock` | `3` |
| `Main/pak4.pk3` | `scripts/pain.shock` | `0.3` |
| `Main/pak4.pk3` | `scripts/stop.shock` | `0` |

`scripts/groggy.shock` carries a second, commented copy of the default block
(lines 30-57) that ends with `//cg_shock_viewKickFadeTime "3"`. The parser never
sees commented lines, but the copy mirrors `scripts/default.shock` and the dead
name is removed from it as well so the block keeps mirroring the corrected
default file.

The affected archives were byte-for-byte identical to the pristine releases
before the fix, so the dead assignment is shipped data and not an artifact of
this installation. `Main/pak4.pk3` matched both the Polish 1.51b and the
English 1.51 references:

```
Main/pak4.pk3  0c8b9cae5f76fe2fedb48f53f7b1b38b7af315d84bf2f092ab7883129c7ce73b
```

## Fix

`tools/patch-coduo-game-data.py` gains the `shockcvar` fix. It removes the
exact `cg_shock_viewKickFadeTime "<value>"` line, and in `groggy.shock` the
commented mirror line as well, from the entries listed above. Each removal is
checked: a line that occurs more than once aborts the archive, an already
absent line is accepted so that a partially applied tree still converges, and
any remaining `cg_shock_viewKickFadeTime` text after the removals also aborts.
The archive is then re-read, CRC-checked, the entry list is compared, and the
planner is re-run against the rewritten archive before it replaces the
original:

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix shockcvar
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix shockcvar --apply --backup
```

Nothing else changes: the remaining 27 assignments, their order, and the file's
CRLF line endings are preserved byte for byte.

## Validation

- Dry run reported exactly the seven `.shock` entries in the installation and
  in the repaired `_zzzz` tree: six files in `Main/pak4.pk3` and
  `scripts/default_mp.shock` in `uo/pakuo01.pk3`.
- Archive hashes after `--apply --backup`:

```
Main/pak4.pk3   0c8b9cae5f76fe2fedb48f53f7b1b38b7af315d84bf2f092ab7883129c7ce73b
             -> 01a252515e9e2a31035146ab63f7b57a1b0d9464e8c21572ecf9098add095260
uo/pakuo01.pk3  a2b3e11fa601574038604cfd15cf19f589595c8ee1ea355c70fffea92f853622
             -> ce861151a97b30d8d12f3ac1a2bba6f869429cf2ec14e85ead5d6277dc4bd277
```

  (`pakuo01` already carried the earlier quote fix, so its "before" value is the
  post-quote-fix archive.)
- Comparing every patched entry with its backup shows exactly one removed line
  per file (two in `groggy.shock`: the assignment and its commented mirror) and
  no added or reordered lines. File sizes drop from 861/877/860/859/1775/879/819
  bytes to 830/846/829/828/1711/846/788.
- Re-running the fix reports `no archive needed a change`, so it is idempotent.
- Started the repaired client with `+map mp_foy`, `developer 1` and
  `logfile 1` for 45 seconds. Before the fix the log contained
  `^3WARNING: unknown cvar 'cg_shock_viewKickFadeTime' in file 'scripts/default_mp.shock'`
  between the `cg_shock_viewKickRadius` and `cg_shock_sound` assignments; after
  the fix the log has no `viewKickFadeTime` occurrence and the shellshock block
  reads `cg_shock_viewKickRadius .05` followed directly by `cg_shock_sound 1`.
  The client still loads `pakuo01.pk3` and `pak4.pk3` and reports no shellshock
  or cvar error.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/13_shock_viewkick_fade_time.md`
