# RETRIEVAL objective is permanently disabled by one pre-match use

## Symptom

On a RETRIEVAL server that does not yet have one player on each team, the
objective shows its "press to pick up" hint but pressing the use key does
nothing. After that single attempt the objective can never be picked up again,
even once the match starts and both teams are populated. Only a map restart or
map change re-arms it.

The same symptom appears when testing alone, which makes a correctly installed
RETRIEVAL server look broken.

## Root cause

The shipped `retrieval_think()` runs one thread per objective. The thread waits
for a trigger, then checks the match state:

```text
while(1)
{
    self.trigger waittill ("trigger", other);

    if(!game["matchstarted"])
        return;

    ...
}
```

`waittill("trigger", other)` consumes the player's use event. `return` then
ends the objective thread. Nothing creates a new listener for that objective,
so every later use has no handler. `game["matchstarted"]` becomes true only
after both teams contain a player; a solo or one-team test never reaches that
state before the first use.

The use key and `+activate` binding are correct. This is a game-script logic
defect, not an input or engine problem.

## Fix

The guard must keep the objective thread alive while the match is not started:

```text
if(!game["matchstarted"])
{
    wait 0.5;
    continue;
}
```

`continue` returns to the top of the loop and waits for the next trigger. The
half-second wait prevents a tight loop if the player holds use. The team checks
below the guard are not reached with the stale `other` value, so pre-match
pickup remains impossible.

The patch applies to every shipped copy of `re.gsc`, because the effective copy
depends on the search path and on whether a mod overrides the script:

- `Main/pak5.pk3`, `Main/pak8.pk3`, `Main/pak9.pk3`, `Main/paka.pk3`,
  `Main/pakb.pk3` — `maps/MP/gametypes/re.gsc` (base Call of Duty)
- `uo/pakuo01.pk3`, `uo/pakuo06.pk3` — `maps/mp/gametypes/re.gsc`
  (United Offensive)

Each copy contains the target two-line sequence exactly once. The tool stops if
a copy has zero or multiple matches instead of forcing a replacement.

## Patch tool

`tools/patch-coduo-game-data.py` performs the replacement in place. It is a dry
run unless `--apply` is given, refuses unexpected match counts, preserves entry
order and names, verifies every rewritten archive's CRCs, and re-reads the
patched entry before replacing the original.

```sh
python3 tools/patch-coduo-game-data.py --game-root /path/to/game
python3 tools/patch-coduo-game-data.py --game-root /path/to/game --apply --backup
```

`<game root>` is the directory containing `Main/` and `uo/`. Run it while the
client and server are closed. `--backup` keeps a `.bugfix-backup` copy beside
each changed archive; without it, the printed SHA-256 values identify the
originals.

## Validation

- Dry run found exactly 14 copies (seven per installation tree) and one guard
  occurrence per copy.
- After applying, the modified entry differs from the backup only in the three
  added lines; every archive passes a full CRC check.
- A second dry run reports every copy as already patched.
- A normal populated round, the attacker/defender paths and the dropped and
  delivered objective paths are unchanged because only the pre-match branch was
  modified.

## Rollback and caveats

- Restore each `.bugfix-backup` file over its archive, or restore an original
  game-data copy.
- The change alters the PK3 checksum. If a server uses `sv_pure` and references
  these archives, clients must use the same patched copies; keep a stock
  installation separate for joining unmodified pure servers.
- A server running a mod with its own `re.gsc` (for example an AWE copy) uses
  the mod's script and needs the same guard change there.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/01_retrieval_pickup_matchstarted.md`
