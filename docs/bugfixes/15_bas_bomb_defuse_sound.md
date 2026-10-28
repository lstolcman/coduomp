# Base Assault bomb defusal is silent

## Symptom

In the **Base Assault** game type, planting the bomb plays its sound normally,
but defusing it plays nothing at all. The progress bar still fills, the bomb is
still defused, and no error is printed — the defusal is simply silent for every
player standing near the objective.

## Investigation

### The defuse call

`uo/pakuo01.pk3::maps/mp/gametypes/bas.gsc` runs the base-assault bomb logic.
Line 3207 starts the defusal loop:

```
				other playsound("MP_bomb1_defuse");
				self.status = "bomb_defusing";
```

The plant path in the same file uses a different spelling and works:

```
				other playsound("MP_bomb_plant");     // line 3728
```

The same file also has the two model names, which is where the misspelling
comes from:

```
	self.objectivemodel setmodel("xmodel/mp_bomb1");            // line 3104
	self.objectivemodel setmodel("xmodel/mp_bomb1_defuse");     // line 3768
```

`mp_bomb1_defuse` is the **model** name. The sound call copied the model
suffix, so it asks for an alias named `MP_bomb1_defuse`.

### What the sound alias bank actually defines

`soundaliases/generic_mp_sound.csv` defines the two bomb aliases with the
`MP_bomb_` prefix:

```
MP_bomb_plant,,misc/MP_plant.wav,1,,0.9,1.05,150,2000,,,
MP_bomb_defuse,,misc/MP_defuse.wav,1,,0.9,1.05,150,2000,,,
```

The same file ships in `uo/pakuo03.pk3` and `uo/pakuo07.pk3`; the runtime search
path puts `pakuo07.pk3` first, and both copies contain the identical definition.
No archive defines `MP_bomb1_defuse`.

Search-and-destroy uses the correct name in every variant of its script, for
example `uo/pakuo07.pk3::maps/mp/gametypes/sd.gsc:2388`:

```
				other playsound("MP_bomb_defuse");
```

So the alias `MP_bomb_defuse` is the live, working name, and `MP_bomb1_defuse`
is a one-token typo in the Base Assault script only.

### Why nothing is logged

`trap_Com_PickSoundAlias` resolves the request through
`Com_FindSoundAlias` (`src/sound/alias/sound_alias_lookup.c:33-55`), which walks
the alias hash chain and returns `NULL` for an unknown name.
`CG_PlaySoundAliasByName` (`src/client/cgame/sound/cg_playsoundaliasbyname.c:
118-123`) forwards the result to the shared playback helper, which returns `0`
for a `NULL` alias without printing anything
(`cg_playsoundaliasbyname.c:51-56`). The misspelled request therefore consumes
no channel, produces no audio, and leaves the console log clean, exactly like a
correctly spelled but silent alias.

### Stock defect

The file is byte-identical in both retail reference trees. SHA-256 of
`maps/mp/gametypes/bas.gsc` is
`0d5043c04a2a815d36a43eb8438937a05436582ec1492826050456ad5bd87586`
(126775 bytes) in the English 1.51 tree, the Polish 1.51b tree, and the
unpatched installation. The typo is part of the shipped retail data, not an
installation artifact.

## Fix

`tools/patch-coduo-game-data.py` gains a `basbomb` fix. It rewrites exactly one
`maps/mp/gametypes/bas.gsc` entry and only when it contains the misspelled
request exactly once and the corrected request not at all:

```
BAS_BOMB_BAD  = b'other playsound("MP_bomb1_defuse");'
BAS_BOMB_GOOD = b'other playsound("MP_bomb_defuse");'
```

A second run reports that no archive needs a change, so the fix is idempotent.
`.bugfix-backup` copies are kept when `--backup` is used:

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix basbomb
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix basbomb --apply --backup
```

## Validation

- Dry run reported exactly two archives: `uo/pakuo01.pk3` in the installation
  and in `_zzzz`.
- `--apply --backup` changed `maps/mp/gametypes/bas.gsc` from
  `0d5043c0…d87586` (126775 bytes) to
  `53574e8c…a2a1976` (126774 bytes), and the archive hash from
  `ce861151a97b30d8d12f3ac1a2bba6f869429cf2ec14e85ead5d6277dc4bd277` to
  `2288d8267bf57de0711f405521d9f29cd9fd28ad237fc48f26a24926dc304267`.
- The patched file contains one `other playsound("MP_bomb_defuse");`, one
  `other playsound("MP_bomb_plant");`, and no `MP_bomb1_defuse`.
- Re-running the dry run prints `no archive needed a change`.
- Ran the `_zzzz` client with `+set g_gametype bas +map mp_foy`,
  `developer 1`, and `logfile 1` for 45 seconds. The archive still loads with
  1657 files and the log contains no new error, warning, or script failure
  compared with the previous run.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/15_bas_bomb_defuse_sound.md`
