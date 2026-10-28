# Desktop gamma stays applied after minimizing the game

## Symptom

With a non-default gamma (`r_gamma` other than 1.0) the whole Windows desktop
keeps the game's brightness curve after the game is minimized or loses focus.
Windows, the taskbar and every other application look washed out or too dark
until the game is closed. Restoring the game works, but minimizing it does not
return the desktop to its normal colors.

## Investigation

### The planning note and the real code

`coduomp_plannings/01_minimization_issue_analysis.md` describes three symptoms:
slow/failed minimization, desktop color corruption, and failed restore. Checked
against the current source:

- The Windows path is authoritative on Windows; the SDL event pump is compiled
  only for non-Windows builds, so adding `SDL_WINDOWEVENT_MINIMIZED` handlers
  cannot affect this platform.
- `MainWndProc` (the game window procedure) already receives `WM_ACTIVATE` and
  passes `HIWORD(wParam)` as the minimized flag to `AppActivate`; `SC_MINIMIZE`
  handling is not required for the operating system to minimize the window.
- Minimize and restore timing was measured at about 160 ms in windowed mode and
  about 200 ms in exclusive fullscreen; this environment does not reproduce a
  slow or failed minimize. Restore also works.
- The color corruption **does** reproduce, but its cause is not a missing
  `SDL_WINDOWEVENT_*` case. Windows applies gamma as a **display-wide** ramp
  (`SetDeviceGammaRamp`), and the game never restores the captured desktop ramp
  when its window becomes inactive or minimized. The SDL-only
  `coduomp_gamma_window_focus_changed` helper has no Windows counterpart.

### Reproduction

A small Win32 probe (`GetDeviceGammaRamp` on the desktop device context) was
run against the clean English tree with `r_gamma 2.0`, `r_fullscreen 0`:

| Point | Ramp hash | Meaning |
| --- | --- | --- |
| Desktop before launch | `546107777` | linear desktop ramp |
| Game loaded | `891036247` | game `r_gamma 2.0` ramp |
| After `ShowWindow(SW_MINIMIZE)` | `891036247` | **desktop still uses the game ramp** |
| After `ShowWindow(SW_RESTORE)` | `891036247` | game ramp (correct for the game) |

The minimize transition itself completed in ~160 ms and the window was
restored correctly, so only the gamma behavior is a defect.

In `r_fullscreen 1` and `r_fullscreen 3` this driver did not apply a
non-linear ramp at all (the captured ramp stayed linear), so the defect is
visible in windowed mode and would also affect any fullscreen mode where
native gamma is active.

## Fix

Two minimal additions, both on the Windows path:

1. `renderer/platform_gamma.c` gains
   `coduomp_gamma_focus_changed_compat(qboolean active)`:

   - inactive: restore `rendererOriginalGammaRamp` (captured by
     `GLimp_InitGamma`) on the game monitor's context, or on the desktop
     context when no monitor-specific context exists;
   - active: rebuild and reapply the renderer mapping through
     `R_SetColorMappings()`;
   - returns immediately when `glConfig.deviceSupportsGamma` is false, so it is
     a no-op before the renderer and on software-gamma paths.

2. `system_console.c` calls the helper from `WM_ACTIVATE` (after
   `AppActivate`/the exclusive-mode compat) and from `WM_SIZE` for
   `SIZE_MINIMIZED` / `SIZE_RESTORED` / `SIZE_MAXIMIZED`. The `WM_SIZE` hook is
   the part that catches an OS-driven minimize even when the window was
   already inactive.

No behavior changes on Linux or macOS; the existing SDL
`coduomp_gamma_window_focus_changed` path is untouched. Both new effects carry
`NOT_FROM_ORIGINAL_SOURCE` comments because they are host-compatibility
behavior, not recovered retail code.

## Validation

- Rebuilt the MSVC client and deployed it to the clean English modified tree.
- Re-ran the gamma probe with `r_gamma 2.0`, `r_fullscreen 0`:

  | Point | Ramp hash |
  | --- | --- |
  | Desktop before launch | `546107777` |
  | Game loaded | `891036247` |
  | After minimize | `546107777` (desktop restored) |
  | After restore | `891036247` (game mapping reapplied) |

- Minimize and restore still complete correctly (`IsIconic` true/false,
  ~160 ms).
- The same probe in `r_fullscreen 1` and `r_fullscreen 3` showed no crash and
  no stuck ramp (those modes remained linear in this environment).
- `make policy-check` passes.

### Remaining planning-note items

The note's "slow/failed minimize" and "failed restore" symptoms were not
reproduced here. If they recur on a slower display-mode switch, the next
minimal candidate is to call `ShowWindow(SW_MINIMIZE)` **before**
`ChangeDisplaySettingsExA` in `coduomp_glw_exclusive_focus_changed_compat`
(with a reentrancy guard, because the synchronous `WM_SIZE`/`WM_ACTIVATE`
messages call back into the same helper). That change is intentionally left out
of this commit: it changes sequencing rather than a proven stuck state, and the
measured latency here is already ~200 ms.

## Files

- `src/client/engine/renderer/platform_gamma.c`
- `src/client/engine/renderer/platform_gamma.h`
- `src/client/engine/system_console.c`
- `docs/bugfixes/19_desktop_gamma_focus.md`
