# Windows offers to close the game while a map is loading

## Symptom

While a map loads, Windows sometimes replaces the game window with a ghost
window titled like the game but marked "Not Responding" (in Polish Windows,
"brak odpowiedzi") and shows the "End now / Close the program" dialog. If the
dialog is ignored, the map finishes loading and the game is perfectly usable.

## Investigation

The map load runs on the window thread and performs long blocking work: BSP
and collision loading, lightmaps, models, shader compilation and asset
registration. The message loop is not serviced during those stretches.

The recovered loading hooks deliberately service only `WM_POWERBROADCAST`:

- `Sys_PumpEvents` in `system_event.c` peeks and dispatches at most one
  `WM_POWERBROADCAST` per call;
- `coduomp_loading_keepalive` in `system_platform.c` performs the same
  power-broadcast-only pump plus a two-millisecond yield.

That is the original 2004 behavior. Windows Vista and later added ghost-window
handling: when a foreground window does not pump messages for about five
seconds, the Desktop Window Manager shows a copy of it with a "(Not
Responding)" title and offers to terminate the process. The 2004 client
predates this feature, so a load that happens to exceed the threshold triggers
it even though the load will complete normally.

## Fix

`coduomp_disable_windows_ghosting_compat()` is added to `system_platform.c` and
called once from `WinMain` in `system_main.c`, before the game window exists.
It resolves `DisableProcessWindowsGhosting` from `user32.dll` with
`GetProcAddress` instead of importing it directly, so the executable still
starts on Windows versions that do not export the entry point (the stock
binaries support older systems).

```
typedef VOID(WINAPI *fn)(VOID);
HMODULE user32 = GetModuleHandleA("user32.dll");
fn disable = (fn)(void *)GetProcAddress(user32, "DisableProcessWindowsGhosting");
if (disable != NULL) disable();
```

The call is a process-wide compatibility switch. It does not make the load
faster and does not change game behavior; it only removes the system's
"terminate the unresponsive program" ghost window while the intentional
blocking load runs.

### Why not pump all messages during the load

`coduomp_loading_keepalive` is invoked from the collision and filesystem
loading loops, and `Sys_PumpEvents` is invoked from `SCR_UpdateScreen` and
`R_LoadSurfaces`. Draining arbitrary messages there would dispatch
`WM_ACTIVATE`, `WM_SIZE`, `WM_PAINT`, `WM_CLOSE` and input events while the
renderer, collision loader or cgame is mid-initialization. Those handlers call
back into the renderer, window-mode changes and gamma code, so the added
re-entrancy risk is much larger than the benefit. The original power-broadcast
pump is retained unchanged.

## Validation

- Rebuilt the MSVC client and deployed it to both clean modified trees.
- Loaded `mp_foy` and `mp_berlin`; the client reaches
  `LOADING... maps/mp/mp_foy.bsp` / `mp_berlin.bsp` and starts normally, with
  no new log errors.
- A window-enumeration probe polled every 500 ms during the load and found no
  ghost/“(Not Responding)” window before or after the change; the ghost is
  timing-dependent and was not forced on this machine. The fix is the
  documented Windows mechanism for exactly this condition.
- `make policy-check` passes.

## Files

- `src/client/engine/system_platform.c`
- `src/client/engine/system_platform.h`
- `src/client/engine/system_main.c`
- `docs/bugfixes/20_windows_ghost_window.md`
