# Inactivity kick ignores mouse-look

## Symptom

A player who keeps looking around with the mouse but does not press a movement
key or fire is warned and then kicked for inactivity when the server sets
`g_inactivity` or `g_inactivityspectator`. Aiming, tracking a target and turning
do not count, even though the player is clearly present.

## Root cause

Both server inactivity timers reset only on movement axes or the attack button:

```c
} else if (client->command.forwardmove != 0 ||
           client->command.rightmove != 0 ||
           client->command.upmove != 0 ||
           (client->command.buttons &
            INACTIVITY_INPUT_BUTTON_MASK) != 0) {
```

`INACTIVITY_INPUT_BUTTON_MASK` is `0x01` (attack). Mouse-look changes only
`usercmd_t.angles`, so a player who only turns never refreshes the deadline.
The spectator timer has the same condition plus `spectatorActivityState`.

## Fix

The engine already keeps the previous per-client command in
`gclient_t.oldPmoveCommand`, updated by `ClientThink` immediately before the
new command is fetched. Comparing the absolute view-angle words of the current
and previous command is therefore a reliable per-command activity signal:

```c
static qboolean ClientCommandChangedViewAngles(const usercmd_t *command,
                                               const usercmd_t *previous)
{
    return command->angles[0] != previous->angles[0] ||
           command->angles[1] != previous->angles[1] ||
           command->angles[2] != previous->angles[2] ? qtrue : qfalse;
}
```

Both `ClientInactivityTimer` and `ClientSpectatorInactivityTimer` now treat a
changed angle triple as activity. The existing movement and attack checks are
unchanged, as are the warning and drop timing.

The angle comparison is against the immediately preceding command, not against
a renderer view angle, so it also covers touchpads and joystick look. Repeated
identical commands (a client that stopped sending new input) do not match and
therefore do not keep an idle player connected.

## Behavior and branch

This is intentional improved behavior, not a stock transcription fix. It is
implemented on the improved line only; the stock line keeps the original
movement-plus-attack rule.

A player who is dead may still avoid the kick by looking around, because the
timer runs for connected players regardless of life state. That matches the
purpose of the timer as a presence check rather than an anti-camping rule.

## Validation

- Built the 32-bit client and `uo_game_mp_x86.dll` with MSVC.
- The helper is pure integer comparison over the existing `usercmd_t` fields;
  no new state, allocation or ABI change is introduced.
- `usercmd_t.angles` are absolute angle words produced by the client and already
  consumed by `BG_UpdateViewAngles`; no other subsystem writes
  `oldPmoveCommand` after `ClientThink` stores it.
- Manual check: set `g_inactivity 1` on a local server, join, and move only the
  mouse for longer than the timeout. The warning and drop must not appear.
  Moving nothing at all must still warn and drop.
- Rebuild and redeploy the game module as well as the engine. Both inactivity
  timers live in the game module, so an existing `game.mp.uo.*.so` or
  `uo_game_mp_x86.dll` keeps the old behavior even when the engine binary is
  new. A server that loads the previous module still drops mouse-only players.

## Files

- `src/server/game/client_frame.c`
- `docs/bugfixes/28_inactivity_mouselook.md`
