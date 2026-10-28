# Multiplayer skies ask for sun files that were never shipped

## Symptom

Starting several stock multiplayer maps prints this warning while the renderer
loads the map lights:

```
^3WARNING: couldn't load sun file 'scripts/mp_<map>.sun'
```

The map still loads, but the sky shader's sun data is missing: the renderer
never gets the sun sprite or flare parameters for that sky. The affected maps
are `mp_arnhem`, `mp_streets`, `mp_depot`, `mp_uo_depot`, `mp_powcamp`,
`mp_uo_powcamp`, `mp_bocage`, `mp_pavlov`, `mp_railyard`, `mp_rocket` and
`mp_ship`.

## Investigation

### Where the name comes from

Each sky shader names its sun data with a `sunfile` directive. The renderer
resolves it as `scripts/<value>.sun`. For example
`Main/pak9.pk3::scripts/sky.shader`:

```
textures/sky/mp_ship
{
    skyParms env/stormyfogged 512 -
    sunfile mp_ship
}
```

The retail tree ships `scripts/ship.sun`, not `scripts/mp_ship.sun`, so the
lookup fails.

`uo/pakuo01.pk3::scripts/gmi_sky.shader` has the same problem for the Arnhem
sky that `mp_arnhem` and `mp_streets` use:

```
textures/sky/mp_arnhem
{
    skyParms env/bastogne1 512 -
    sunfile mp_hurtgen
}
```

Only `scripts/mp_carentan.sun`, `scripts/mp_dawnville.sun`, `scripts/mp_harbor.sun`
and `scripts/mp_sicily.sun` actually shipped with an `mp_` name. The rest of
the multiplayer skies were given multiplayer names but the matching files were
never added; the single-player files with the same lighting data are present.

### Map to shader to sun file

`scripts/sky.shader` is present in `Main/pak4.pk3` (1.4) and
`Main/pak9.pk3` (1.5); the 1.5 copy is the effective one and contains the full
51-shader set. `scripts/gmi_sky.shader` is the UO addition and wins for its own
sky names. Parsing the shader blocks and the `textures/sky/*` shader names used
by each `maps/mp/*.bsp` gives this table:

| Map(s) | Sky shader | Requested | Shipped equivalent |
| --- | --- | --- | --- |
| `mp_arnhem`, `mp_streets` | `textures/sky/mp_arnhem` | `mp_hurtgen` | `scripts/hurtgen.sun` |
| `mp_depot`, `mp_uo_depot` | `textures/sky/mp_depot` | `mp_depot` | `scripts/dam.sun` (same `env/dam` sky) |
| `mp_powcamp`, `mp_uo_powcamp` | `textures/sky/mp_powcamp` | `mp_powcamp` | `scripts/powcamp.sun` |
| `mp_bocage` | `textures/sky/mp_bocage` | `mp_brecourt` | `scripts/brecourt.sun` |
| `mp_pavlov` | `textures/sky/mp_pavlov` | `mp_pavlov` | `scripts/pavlov.sun` |
| `mp_railyard` | `textures/sky/mp_railyard` | `mp_railyard` | `scripts/railyard.sun` |
| `mp_rocket` | `textures/sky/mp_rocket` | `mp_rocket` | `scripts/rocket.sun` |
| `mp_ship` | `textures/sky/mp_ship` | `mp_ship` | `scripts/ship.sun` |

The single-player counterpart of each sky proves the intended data. In
`Main/pak9.pk3::scripts/sky.shader`:

```
textures/sky/dam       { skyParms env/dam          512 -  sunfile dam }
textures/sky/brecourt  { skyParms env/hedgerow     512 -  sunfile brecourt }
textures/sky/pavlov    { skyParms env/pavlovtest   512 -  sunfile pavlov }
textures/sky/powcamp   { skyParms env/powcamp      512 -  sunfile powcamp }
textures/sky/railyard  { skyParms env/pavlovtest   512 -  sunfile railyard }
textures/sky/rocket    { skyParms env/stormyfogged 512 -  sunfile rocket }
textures/sky/ship      { skyParms env/shipnight    512 -  sunfile ship }
textures/sky/hurtgen   { skyParms env/stormyfogged 512 -  sunfile hurtgen }
```

`textures/sky/mp_depot` uses the same `env/dam` panorama as `textures/sky/dam`,
so `dam` is the matching sun file. `textures/sky/mp_bocage` deliberately reuses
the Brecourt sun (`mp_brecourt`), and the corrected name keeps that reuse.

No multi-player BSP uses `textures/sky/stalingrad`, whose `sunfile staligrad`
misspells `stalingrad.sun`; that is a single-player-only defect and is left
alone here.

### Why it is stock data

The same `sunfile mp_*` values are in the clean English and Polish 1.51b
reference trees, and the string data is identical in the older working tree.
Retail prints the same warning; the matching `.sun` files simply were not added
to the multiplayer pak.

## Fix

`tools/patch-coduo-game-data.py` gains an `mpsun` fix that rewrites the
`sunfile` directives in `scripts/sky.shader` and `scripts/gmi_sky.shader`:

```
sunfile mp_hurtgen   -> sunfile hurtgen
sunfile mp_depot     -> sunfile dam
sunfile mp_brecourt  -> sunfile brecourt
sunfile mp_pavlov    -> sunfile pavlov
sunfile mp_powcamp   -> sunfile powcamp
sunfile mp_rocket    -> sunfile rocket
sunfile mp_railyard  -> sunfile railyard
sunfile mp_ship      -> sunfile ship
```

The planner rewrites every occurrence in the two shader files, refuses to
finish if any listed name remains, and skips files without a listed name, so it
is idempotent. Both the 1.4 (`pak4`) and 1.5 (`pak9`) copies are patched; only
`pak9` is loaded on a 1.5 installation.

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix mpsun
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix mpsun --apply --backup
```

## Validation

- Dry run changed `Main/pak4.pk3::scripts/sky.shader`,
  `Main/pak9.pk3::scripts/sky.shader` and
  `uo/pakuo01.pk3::scripts/gmi_sky.shader` in both clean trees.
- Entry hashes before → after:

  | Entry | Before | After |
  | --- | --- | --- |
  | `pak4 scripts/sky.shader` | `a503d54f…4f05515b` (11912 B) | `1d07f40d…771c46e1f` (11886 B) |
  | `pak9 scripts/sky.shader` | `4e446431…aef24fc2` (12350 B) | `a426e489…c68b95c04bf` (12321 B) |
  | `pakuo01 scripts/gmi_sky.shader` | `5d012219…171227e4` (8436 B) | `3cb6bbea…3fc2c1ac45` (8433 B) |

- After patching, every remaining `sunfile` name in these files resolves to a
  shipped `scripts/<name>.sun`; the four `mp_*` names that remain
  (`mp_carentan`, `mp_dawnville`, `mp_harbor`, `mp_sicily`) all ship.
- Re-running the fix prints `no archive needed a change`.
- Ran the clean English tree with our rebuilt client on `mp_arnhem` and
  `mp_ship` (`+set g_gametype tdm`, `developer 1`, `logfile 1`, 40 s each).
  Both reached `Loading lights...` with no `couldn't load sun file` warning.
  `mp_ship` then set `r_sunsprite_shader gfx/sun/moon_full` and
  `r_sunsprite_size 32` from the now-loaded ship sun data.
- No screenshot comparison is stored for this fix: the automated spawn view of
  `mp_pavlov` frames an overcast sky with no visible sun sprite, so a still
  image cannot demonstrate the difference. The console warning and the
  `r_sunsprite_*` values are the evidence.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/18_mp_sun_files.md`
