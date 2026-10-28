# Arnhem loads a missing explosion effect on every start

## Symptom

Every start of the multiplayer map `mp_arnhem` prints this line in the client
console and `console_mp.log`:

```
^1Effect file load failed: fx/map_mp/mp_exp_generic_complete.efx: file not found
```

The failure happens during the map's effect precache, before anyone fires a
shot. The precached handle `level._effect["antitankgunexplosion"]` stays zero,
so any script that starts it plays nothing.

## Investigation

### The request

`uo/pakuo00.pk3::maps/mp/mp_arnhem_fx.gsc` runs from the map script
(`maps/mp/mp_arnhem.gsc:10` starts `mp_arnhem_fx::main()`), and its precache
function requests the effect:

```
 10 	level._effect["antitankgunexplosion"]	= loadfx("fx/map_mp/mp_exp_generic_complete.efx");
```

The map script `mp_arnhem.gsc` never reads that key, so the effect is currently
dead data — but the precache still runs and still fails on every start.

### The file is not shipped

`pakuo00.pk3` contains the other `fx/map_mp/` effects (`mp_arnhem_fire`,
`mp_arnhem_fire_smolder`, `mp_arnhem_fog`, `mp_arnhem_fog_sm`,
`mp_arnhem_smoke`) but not `mp_exp_generic_complete.efx`. No archive in the
clean English or Polish 1.51b tree contains that name, in `fx/` or anywhere
else; the archive entry lists are identical between the two clean trees and the
older working tree for `pakuo00.pk3`.

### Why it is a data defect and not a port defect

`CFxScheduler::RegisterEffect` in
`src/client/engine/effects/fx_scheduler.cpp` opens the registration name
verbatim through `SFxHelper_OpenFile` → `FS_FOpenFileByMode` and has no
fallback path:

```
 280     const int32_t fileLength = SFxHelper_OpenFile(fileName, &fileHandle, 0);
 282     if (fileLength <= 0) {
 287         SFxHelper_Print("^1Effect file load failed: %s: file not found\n", fileName);
 290         return 0;
```

The loader deliberately matches the original Windows client: a name that is not
present on disk is reported and the effect id is zero. Retail prints the same
message for this map; the defect is in the shipped map data.

### What the intended effect is

- `lofx/map_mp/mp_generic_complete.efx` **does** ship. It is the low-detail
  twin of this effect: its two particles are named `smoke_linger_mist` and
  `fire_exp_core`, exactly the particles of the full-detail
  `fx/explosions/vehicles/generic_complete.efx`, with fewer copies and shorter
  lives.
- `fx/explosions/vehicles/generic_complete.efx` ships in `pakuo00.pk3`
  (10979 bytes) and is the UO full-detail "generic complete" explosion. UO uses
  it for destroyed turrets, vehicles and bombs:
  `maps/_bombs_gmi.gsc:45-107`, `maps/bastogne2_fx.gsc:18`,
  `maps/_scripted_turrets_gmi.gsc:235`, `:581`.
- The base-game scripts that define the same `antitankgunexplosion` key use
  `fx/explosions/explosion1_nolight.efx`
  (`Main/pak4.pk3::maps/MP/mp_carentan_fx.gsc:8`,
  `Main/pak5.pk3::maps/MP/mp_hurtgen_fx.gsc:8`,
  `Main/pak5.pk3::maps/MP/mp_pavlov_fx.gsc:8`).

The requested name looks like a hand-edited merge of the map-local
`mp_generic_complete` and the generic explosion, and the full-detail map copy
was never shipped. The only shipped full-detail effect with the same particles
and the same role is `fx/explosions/vehicles/generic_complete.efx`, so the map
is pointed there.

## Fix

`tools/patch-coduo-game-data.py` gains an `arnhemfx` fix:

```
ARNHEM_FX_BAD  = b'loadfx("fx/map_mp/mp_exp_generic_complete.efx")'
ARNHEM_FX_GOOD = b'loadfx("fx/explosions/vehicles/generic_complete.efx")'
```

It rewrites exactly one `maps/mp/mp_arnhem_fx.gsc` entry, refuses to write
unless the old request occurs once and the new one zero times, and skips an
already patched archive, so it is idempotent.

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix arnhemfx
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix arnhemfx --apply --backup
```

## Validation

- Dry run changed one archive in each clean tree:
  `uo/pakuo00.pk3`.
- `maps/mp/mp_arnhem_fx.gsc` changed from
  `3748f07e…27a327f744` (3511 bytes) to
  `272389cc…7d924e7f1` (3517 bytes); `pakuo00.pk3` changed from
  `20bbcc03…3ebb60f7` to `f35e19b0…3e631b32`. The English and Polish clean
  trees are byte-identical for this archive, so both produced the same hashes.
- Re-running the fix prints `no archive needed a change`.
- Started the clean English tree with our rebuilt client,
  `+set g_gametype tdm +map mp_arnhem`, `developer 1`, `logfile 1`, for 40
  seconds. Before the fix the log contained
  `^1Effect file load failed: fx/map_mp/mp_exp_generic_complete.efx: file not
  found`; after the fix the log contains no `Effect file load failed`, no
  script error, and the map reaches `LOADING... maps/mp/mp_arnhem.bsp` and
  starts normally.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/17_mp_arnhem_missing_effect.md`
