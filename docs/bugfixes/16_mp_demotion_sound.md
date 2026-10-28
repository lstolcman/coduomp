# Multiplayer demotion is silent

## Symptom

Battle rank is on by default (`scr_battlerank` defaults to `1`), and losing
points correctly lowers a player's rank: the head icon, status icon and rank
number all drop, and the `GMI_RANK_DEMOTION` message appears. The demotion
**sound** never plays. There is no error, no warning, and no console message —
the promotion jingle plays when a player ranks up, but a demotion is silent.

## Investigation

### The demotion call

`uo/pakuo01.pk3::maps/mp/gametypes/_rank_gmi.gsc` owns battle rank. The round
loop in `bas.gsc:2911-2913` polls for rank changes whenever battle rank is on:

```
		// check all the players for rank changes
		if ( getCvarint("scr_battlerank") )
			maps\mp\gametypes\_rank_gmi::CheckPlayersForRankChanges();
```

`CheckPlayersForRankChanges` compares the stored rank with the rank the score
currently earns and branches on the direction:

```
 266 			old_rank = player.pers["rank"];
 267 			new_rank = DetermineBattleRank(player);
 269 			if ( old_rank != new_rank )
 271 				// did they get promoted?
 272 				if ( old_rank < new_rank )
 274 					player notify("rank changed");
 275 					PlayPromotionSound(player);
 278 				// or demoted?
 279 				else
 281 					player notify("rank changed");
 282 					PlayDemotionSound(player);
```

`PlayDemotionSound` requests the alias `mp_demotion` on both team branches:

```
 340 PlayDemotionSound(player)
 342 	if ( player.pers["team"] == "allies" )
 344 		player playLocalSound("mp_demotion" );
 347 	else
 348 		player playLocalSound("mp_demotion");
```

### Demotion is reachable, not dead code

`DetermineBattleRank` (`_rank_gmi.gsc:217-245`) derives the rank from
`player.score`, with the default thresholds `br_rank_1`…`br_rank_4` =
`br_ppr * 1…4` and `br_ppr` = `scr_rank_ppr` = 10, so ranks change every ten
points.

`player.score` goes **down** as well as up. Base Assault applies the negative
scoring values and copies the result back into the entity score:

```
1448 				attacker.pers["score"] += maps\mp\gametypes\_scoring_gmi::GetSuicidePoints();
1462 					attacker.pers["score"] += maps\mp\gametypes\_scoring_gmi::GetTeamKillPoints();
1499 		attacker.score = attacker.pers["score"];
```

`InitializeBattleRank` sets `br_points_suicide` = `-1` and `br_points_teamkill`
= `-3` (`_rank_gmi.gsc:36-39`), and `ValidateScore` only clamps the result. So a
player who has just crossed a threshold and then suicides or team-kills loses
rank on the next `GameRoundThink` pass, and `PlayDemotionSound` runs. The
promotion path (`PlayPromotionSound` → `mp_promotion`) proves the mechanism is
live; only the demotion name is broken.

### What the alias table actually defines

`soundaliases/generic_mp_sound.csv` in `uo/pakuo03.pk3` and `uo/pakuo07.pk3`
(the latter wins the runtime search path) has this row at line 44, under the
`#MP Demotion` comment:

```
mop_demotion,,music/mp_promotion.mp3,1,,,,,,,,music,streamed
```

Two independent defects on one row:

1. The name is `mop_demotion`, not `mp_demotion`. `m` and `p` were transposed.
2. The file is `music/mp_promotion.mp3`, the promotion jingle, not the demotion
   music. Both mistakes look like a copy of the `mp_promotion` row above it
   (line 41) with the suffix edited by hand.

The correct data already exists in the same table:

```
32 ge_demotion,,music/mp_demotion.mp3,1,,,,,,,,music,streamed,,,,,
35 uk_demotion,,music/mp_demotion.mp3,1,,,,,,,,music,streamed
```

and the file itself ships at `sound/music/mp_demotion.mp3` (79833 bytes) in
`uo/pakuo02.pk3`. For comparison, `sound/music/mp_promotion.mp3` in the same
archive is 41276 bytes, so even if the name had matched, a demotion would have
played the wrong, shorter cue.

### Why nothing is logged

`playLocalSound` resolves the name through `trap_Com_PickSoundAlias` →
`Com_FindSoundAlias` (`src/sound/alias/sound_alias_lookup.c:33-55`), which walks
the alias hash chain and returns `NULL` when no row defines the name.
`CG_PlaySoundAliasByName`
(`src/client/cgame/sound/cg_playsoundaliasbyname.c:118-123`) forwards that
`NULL` to the shared playback helper, which returns `0` without printing
anything (`cg_playsoundaliasbyname.c:51-56`). A misspelled alias is therefore
indistinguishable from a correctly spelled silent one: no audio, no channel,
no log line.

### Stock defect

The row is identical in the English 1.51 tree, the Polish 1.51b tree, the
unpatched installation and `_zzzz`. It is shipped retail data, not an
installation artifact.

## Fix

`tools/patch-coduo-game-data.py` gains a `demotionsound` fix that rewrites one
row in `soundaliases/generic_mp_sound.csv`:

```
DEMOTION_BAD  = b"mop_demotion,,music/mp_promotion.mp3,1,"
DEMOTION_GOOD = b"mp_demotion,,music/mp_demotion.mp3,1,"
```

It requires exactly one mistyped row and no corrected row before writing, and
skips an archive that already has the corrected row, so it is idempotent. The
leading `1,` is part of the pattern so the volume field is preserved and the
match cannot spill into a different row.

The rank script is deliberately left unchanged: it already uses the correct
alias name, and the alias table is the only place where the typo exists.

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix demotionsound
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix demotionsound --apply --backup
```

## Validation

- Dry run reported exactly four archives: `uo/pakuo03.pk3` and
  `uo/pakuo07.pk3` in the installation and in `_zzzz`.
- `--apply --backup` rewrote `soundaliases/generic_mp_sound.csv` in all four:
  - `pakuo03`: `6910f8f0…a2baa89d` (189265 bytes) →
    `9cf021f5…226a2067` (189263 bytes); archive `d16dc9a2…f193104` →
    `36dbf631…ad18224`.
  - `pakuo07`: `ba39353a…a04db25` (189261 bytes) →
    `516d5331…95f6612d` (189259 bytes); archive `61dcbfa7…bdcb60d` →
    `4d71bcbf…88a59ac`.
  - The two bytes are the `mop`→`mp` and `promotion`→`demotion` length
    differences.
- Each patched file contains exactly one
  `mp_demotion,,music/mp_demotion.mp3,1,…` row, zero `mop_demotion`, and still
  contains the `ge_demotion` and `uk_demotion` rows unchanged.
- Re-running the dry run prints `no archive needed a change`.
- Ran the `_zzzz` client with `+set g_gametype tdm +map mp_foy`, `developer 1`
  and `logfile 1` for 45 seconds. The sound alias bank loads without a parse
  or unknown-channel warning (`Sound alias strings use 40.0 KB`), and the log
  has no new error compared with the previous run.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/16_mp_demotion_sound.md`
