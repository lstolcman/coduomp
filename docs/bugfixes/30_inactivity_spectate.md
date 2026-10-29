# Move inactive players to spectators instead of dropping them

## Feature

`g_inactivity` keeps its original meaning: the number of seconds a connected
player may go without input before the server acts. The new
`g_inactivityAction` cvar selects that action:

| Value | Behavior |
| --- | --- |
| `kick` (default) | Drop the client with `GAME_DROPPEDFORINACTIVITY`, as before. |
| `spectate` | Move the client to the spectator team and keep the connection open. |

```cfg
set g_inactivity 60
set g_inactivityAction "spectate"
set g_inactivityspectator 600
```

With `spectate`, the player leaves the match as a normal spectator. The stock
spectator timer (`g_inactivityspectator`) still applies, so an operator can
combine "move idle players out of the match" with "drop idle spectators after a
longer delay". Setting `g_inactivityspectator 0` keeps them connected as
spectators indefinitely.

## Implementation

`ClientInactivityTimer` calls a new `ClientMoveToSpectatorOnInactivity` helper
when the deadline expires and `g_inactivityAction` is `spectate`. The helper:

- releases a mounted turret or vehicle before changing state, using the same
  cleanup gates as `ClientSpawn`;
- sets `clientState.team = TEAM_SPECTATOR` and
  `sessionState = SESS_STATE_SPECTATOR`, which is the state the gametype scripts
  assign for a normal spectator;
- clears follow/archive state and the vehicle and locked-view player-state
  fields;
- marks the entity invisible and unlinks it so it leaves collision;
- resets the spectator inactivity timer so the spectator policy starts from a
  clean deadline.

Unknown cvar values fall back to `kick`, so a malformed configuration cannot
leave an inactive client permanently connected. The player can rejoin a team
through the normal team menu, which counts as activity and removes the
spectator state.

## Validation

- Built the native CMake GCC server and game module with the faithful x87
  policy (`-O0 -mfpmath=387 -fexcess-precision=fast`).
- Started `mp_arnhem` on the unmodified stock tree with `sv_pure 1`,
  `g_inactivity 1`, and `g_inactivityAction spectate`; the server reached
  `Game Initialization` and ran without errors.
- The default `kick` path is unchanged; `g_inactivityAction` defaults to
  `kick` in the cvar table.

A live-client check needs a connected client: wait past `g_inactivity` without
input and confirm the player appears in the spectator list instead of
disconnecting. That check requires the rebuilt game module (`game.mp.uo.*.so`
or `uo_game_mp_x86.dll`), not only a rebuilt engine.

## Files

- `src/server/game/client_frame.c`
- `src/server/game/game_globals.c`
- `src/server/game/game_globals.h`
