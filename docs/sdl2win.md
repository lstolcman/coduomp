# SDL2 window and rendering migration (Windows)

This document records the migration of the Windows client from the original
Win32 window, WGL context, GDI display, and window-message implementation to
the same SDL2 application shell that already served Linux and macOS. The
Windows client no longer creates a Win32 game window, a WGL context, a device
context, or a DirectDraw surface. The game window, display mode, OpenGL
compatibility context, presentation, gamma, clipboard, dialogs, and translated
input all pass through `src/client/engine/platform/sdl_platform.c`.

The original Windows binaries remain the behavioural authority. The migration
is a platform substitution, not a gameplay change: the recovered renderer,
renderer cvars, mode table, gamma tables, input bindings, event queue, and
command system are unchanged.

## 1. What was removed

| Removed Win32 responsibility | Previous owner | Replacement |
| --- | --- | --- |
| Game window class, `CreateWindowExA`, `ShowWindow`, `MoveWindow`, `DestroyWindow` | `renderer/platform_wgl.c` | `SDL_CreateWindow` / `SDL_DestroyWindow` |
| Pixel format selection (`DescribePixelFormat`, `ChoosePixelFormat`, `SetPixelFormat`) | `renderer/platform_wgl.c` | `SDL_GL_SetAttribute` + `SDL_GL_CreateContext` |
| WGL context creation, current-context calls, deletion | `renderer/platform_wgl.c` | `SDL_GL_CreateContext`, `SDL_GL_MakeCurrent`, `SDL_GL_DeleteContext` |
| `SwapBuffers` | `renderer/platform_wgl.c` | `SDL_GL_SwapWindow` |
| `wglSwapIntervalEXT` | `renderer/platform_wgl.c` | `SDL_GL_SetSwapInterval` |
| `ChangeDisplaySettingsEx`, `EnumDisplaySettings`, `EnumDisplayDevices`, QueryDisplayConfig preferred-mode queries | `renderer/platform_wgl.c` | `SDL_GetNumVideoDisplays`, `SDL_GetDisplayBounds`, `SDL_GetCurrentDisplayMode`, `SDL_GetNumDisplayModes`, `SDL_GetClosestDisplayMode`, `SDL_SetWindowDisplayMode` |
| `GetDC`/`ReleaseDC`, `SetDeviceGammaRamp`, `GetDeviceGammaRamp`, `ChangeDisplaySettingsA`, `EnumThreadWindows` gamma/window callbacks | `renderer/platform_gamma.c` | `SDL_GetWindowGammaRamp`, `SDL_SetWindowGammaRamp`, `SDL_GetWindowWMInfo` |
| `LoadLibraryA("opengl32.dll")`, `GetProcAddress`, `wglGetProcAddress` dispatch | `renderer/qgl_dispatch.c` | `SDL_GL_GetProcAddress` |
| `CreateProcessA`-style dummy OpenGL video-memory probe, DirectDraw/`IDirectDraw7` memory query | `system_video_memory.c` | dynamic DXGI adapter query; hidden SDL OpenGL context renderer-name probe |
| `PeekMessage`/`GetMessage`/`TranslateMessage`/`DispatchMessage` game-window pump, `MainWndProc`, `MapKey` | `system_console.c`, `system_event.c` | `SDL_PollEvent` translation in `CoduoSDL_PumpEvents` |
| Raw Input registration and `GetRawInputData` | `system_input.c` | `SDL_SetRelativeMouseMode` (raw device motion) or SDL cursor recentering when `in_rawInput 0` |
| `SetCursorPos`, `SetCapture`, `ClipCursor`, `ShowCursor` capture | `system_input.c` | `SDL_SetRelativeMouseMode`, `SDL_SetWindowGrab`, `SDL_WarpMouseInWindow` |
| `WinMain` | `system_main.c` | portable `main`; SDL2main supplies the Windows `WinMain` trampoline |
| `RegisterClassA` splash window and `LoadImageA` bitmap control | `system_platform.c` | SDL splash window presenting the same retail `coduo.bmp` |
| `OpenClipboard`/`GetClipboardData(CF_TEXT)` | `system_platform.c` | `SDL_GetClipboardText` |
| `MessageBoxA` fatal, hardware-change, low-memory, and process-lock dialogs | `system_fatal.c`, `system_info.c`, `system_cpu.c`, `system_process_lock.c`, `renderer/platform_glw.c` | `SDL_ShowSimpleMessageBox` and `SDL_ShowMessageBox` |

`renderer/platform_wgl.c` is now `renderer/platform_glw.c`. It contains the
recovered `GLW_*` mode policy and OpenGL extension inventory only; every
platform call inside it targets SDL.

## 2. Application lifetime

`system_main.c` now has exactly one entry point for every host:

1. Restore the retail x87 precision mode.
2. Windows only: opt into per-monitor DPI awareness, raise the CRT stream
   limit, resolve the executable module handle for `sysApplicationInstance`.
3. Build the mutable command line from `argc`/`argv`.
4. Windows only: process-lock check (the `allowdupe` prefix retains its
   original meaning).
5. `CoduoSDL_Init`: video, events, and game-controller subsystems; text input;
   screen-saver inhibition.
6. `Sys_InitHardwareInfo`, executable checksum.
7. `Sys_CreateConsole`, SDL splash creation and presentation.
8. `Com_Init`, renderer startup, and the original frame loop.
9. `atexit(CoduoSDL_Shutdown)` releases SDL after the engine has shut down.

On Windows the executable links `SDL2main`. The real `main` is compiled as
`SDL_main` through `SDL_main.h`; SDL2main's `WinMain` parses the Unicode command
line and calls it. The previous hand-written `WinMain`, including its
`HINSTANCE`/`previousInstance` arguments and `SetFocus(win32MainWindow)` call,
is gone. `sysApplicationInstance` now receives `GetModuleHandleA(NULL)`, which
is the same module base the CRT passed as `HINSTANCE`.

## 3. Window and display-mode policy

`CoduoSDL_CreateOpenGLWindow` is the single native window constructor:

* Requested color depth selects 8/8/8 color and zero alpha, matching the retail
  `PIXELFORMATDESCRIPTOR` which leaves `cAlphaBits` at zero.
* Depth and stencil follow `r_depthbits`/`r_stencilbits`; a depth buffer below
  24 bits disables stencil, as the original pixel-format negotiation did.
* A legacy OpenGL 2.1 compatibility context is requested because the recovered
  renderer uses the fixed-function pipeline.
* `r_fullscreen 0` opens a decorated window positioned at the selected
  display's origin plus `vid_xpos`/`vid_ypos`, clamped so the window remains on
  that display.
* `r_fullscreen 1` uses `SDL_WINDOW_FULLSCREEN_DESKTOP`: the desktop mode stays
  active, other monitors remain usable, and the renderer's backbuffer is
  presented into the full display.
* `r_fullscreen 2` uses a borderless window covering the selected display's
  bounds without entering an OS fullscreen state.
* `r_fullscreen 3` uses `SDL_SetWindowDisplayMode` plus
  `SDL_WINDOW_FULLSCREEN`. An unavailable requested mode fails instead of
  silently selecting a different resolution or refresh rate.

`r_display` is a zero-based SDL display index. Out-of-range values fall back to
display 1 with the original warning. The renderer publishes
`r_currentDisplayLogicalWidth/Height`, `r_currentDisplayBackingWidth/Height`,
`r_currentDisplayBuiltin`, `r_displayCount`, and `r_currentDisplayIndex` from
SDL display queries; the separately linked UI module consumes the same cvars.

`GLW_SetMode` keeps the recovered mode table, windowed-mode shrinking against
the desktop size, automatic-mode (`r_mode -2`) selection, exclusive Fit Screen
stretching, and the 16-bit desktop color-depth advisory. Desktop color depth now
comes from the SDL desktop mode's pixel stride, which reports the same 32-bit
value as the original `dmBitsPerPel` query.

## 4. OpenGL context and dispatch

`QGL_Init` no longer loads `opengl32.dll`. Every core and extension entry point
is resolved through `SDL_GL_GetProcAddress`, and the recovered WGL
`GetProcAddress` slot is bound to that same resolver so the existing
`CODUOMP_WGL_LOAD_SYMBOL` inventory keeps working unchanged.

`CoduoSDL_GetOpenGLFormat` reports the realized framebuffer attributes. On
Windows it reproduces the original `PIXELFORMATDESCRIPTOR.cColorBits` domain:
an 8/8/8/0 context is reported as 32 bits (the padded desktop pixel), so
`PickInternalFormat` chooses the same four-channel color-depth texture format
as the Win32 build. Linux and macOS continue to report the summed channel
widths, which is their established behaviour.

The typed WGL entry slots in `qgl_wgl_entries.h` and the logging/error wrappers
in `wgl_debug_wrappers.c` are retained as inert dispatch storage. No WGL
context is created and the slots remain `NULL`; the logging and error
controllers therefore install no WGL wrappers. `WGL_EXTENSIONS` is empty, as it
already was on the other hosts.

## 5. Presentation, gamma, and focus

`GLimp_EndFrame` presents with `SDL_GL_SwapWindow`. `r_swapInterval` is applied
with `SDL_GL_SetSwapInterval` whenever the cvar changes.

Gamma is a per-display ramp in SDL on every host:

* `GLimp_InitGamma` captures the ramp through `SDL_GetWindowGammaRamp`.
* `GLimp_SetGamma` builds the same byte-expanded, monotonic 16-bit ramp and
  applies it with `SDL_SetWindowGammaRamp`.
* If the native ramp is unavailable or rejected, the existing exact OpenGL
  output LUT fallback activates.
* Focus transitions restore the captured desktop ramp while another window has
  focus and rebuild the game ramp on return, matching the recovered Win32
  `WM_ACTIVATE`/`WM_SIZE` policy.
* `RestoreSystemGammas` remains the fatal-error boundary. `GLimp_Shutdown`
  restores the captured desktop ramp before the SDL window is destroyed; while
  the window still exists, the fatal path can also install the original linear
  ramp. SDL restores the desktop display mode itself when the window is
  destroyed, replacing `ChangeDisplaySettingsA(NULL, 0)`.

The Windows client still rejects display gamma in true fullscreen on some
adapters (including remote sessions). Both the old and new builds log the
rejection and use the software LUT fallback, so observed output is unchanged.

## 6. Input translation

`CoduoSDL_PumpEvents` translates SDL records into the recovered `sysEvent_t`
queue:

* `SDL_KEYDOWN`/`SDL_KEYUP` -> `SE_KEY` with the original engine key domain
  (letters, digits, F1-F12, keypad, arrows, modifiers, mouse buttons, wheel).
* `SDL_TEXTINPUT` -> `SE_CHAR`. UTF-8 is decoded to Unicode; Windows converts
  each character through the active ANSI code page (`CP_ACP`), matching the
  original `WM_CHAR` byte domain, while other hosts accept Latin-1 code points
  as their established compatibility domain.
* `SDL_MOUSEMOTION` -> `SE_MOUSE`. Relative mode supplies unaccelerated device
  motion, the SDL equivalent of the retired Raw Input path.
* `SDL_MOUSEBUTTONDOWN`/`UP` -> `SE_KEY` for `K_MOUSE1`..`K_MOUSE5`.
* `SDL_MOUSEWHEEL` -> `MWHEELUP`/`MWHEELDOWN` press/release pairs.
* `SDL_WINDOWEVENT_CLOSE` -> `Com_Quit_f`.
* `SDL_WINDOWEVENT_FOCUS_GAINED`/`LOST` -> `AppActivate` plus gamma restore.
* `SDL_WINDOWEVENT_MINIMIZED`/`RESTORED` -> the recovered gamma and
  `sysWindowMinimized` transitions.
* `SDL_WINDOWEVENT_MOVED` -> `vid_xpos`/`vid_ypos` while windowed.

Two original chords are preserved explicitly:

* Alt+Enter toggles `r_fullscreen` between windowed and fullscreen and issues
  `vid_restart`, except while loading.
* Ctrl-V (and Command-V on macOS) synthesizes the original character 22 paste
  event; Backspace synthesizes the original `\b` character event.

`in_rawInput` remains archived and visible in the graphics menu. `1` (the
default) selects SDL relative mouse mode. `0` selects the recovered
cursor-recentering fallback: the window is grabbed, the cursor is recentered
with `SDL_WarpMouseInWindow`, and absolute desktop motion is converted to
`SE_MOUSE` deltas. `in_rawInputActive` reports whether a capture path is live.

## 7. Clipboard, dialogs, splash, and icon

* `Sys_GetClipboardData` reads `SDL_GetClipboardText` and applies the original
  newline/carriage-return/backspace truncation.
* `Sys_ShowErrorDialog`, `Sys_OutOfMemory`, the color-depth advisory, and the
  hardware-change reconfigure question use SDL message boxes. The question
  keeps the original OK/Cancel semantics.
* The startup splash presents the same retail `coduo.bmp` in a temporary SDL
  window, centered on the desktop. It is hidden when the renderer window is
  created and destroyed during shutdown, matching the original ordering.
* No icon code is required: SDL's Windows video backend loads the first RT_GROUP_ICON
  resource from the executable, which is still embedded by
  `platform/windows_resources.rc` (resource id 133).

## 8. Hardware profile

`Sys_GetVideoMemoryMB` previously measured local video memory with DirectDraw
and, as a fallback, a temporary WGL context. On Windows it now:

1. queries dedicated adapter memory through DXGI (`CreateDXGIFactory` resolved
   dynamically; no new link dependency);
2. otherwise creates a hidden 1x1 SDL OpenGL window, reads `GL_RENDERER`, and
   applies the recovered 47-entry renderer-memory table;
3. otherwise reports 256 MB, the highest video-memory tier in the shipped
   `configure_mp.csv` recommendation table.

macOS and Linux keep their established 256 MB compatibility value, so their
archived `sys_vidMB` and recommended configuration row do not change.

The power-of-two MB bucketing and the 32 MB overshoot rule are unchanged. On
modern adapters the DXGI dedicated-memory value buckets to the same tier as the
retired DirectDraw query, so `sys_vidMB` and the recommended configuration row
do not change.

## 9. Win32 code retained outside the renderer

The migration removes Win32 window and rendering ownership. The following
platform-local services remain because they are not part of the game's window
or rendering path and have no SDL equivalent:

* The developer console window (`system_console.c`): the original Win32 edit
  control log/console. It is not the game renderer and never owns an OpenGL
  context. SDL's Windows message pump dispatches its messages, including
  keyboard, buttons, copy, clear, quit, and error text.
* WinMM joystick polling and WinMM MIDI input (`system_input.c`): native device
  providers. They never create or present the game window.
* DXGI video-memory enumeration (`system_video_memory.c`): a headless hardware
  query used before the renderer exists.
* `RegisterHotKey`/`SystemParametersInfo` Alt-Tab policy
  (`system_console.c`): invoked by `Sys_GameWindowCreated`; it installs the
  original `win_allowalttab` behaviour without a window procedure.
* Process lock, improper-quit sentinel, executable checksum, DPI awareness,
  WinMM timer period, URL/process launch, and CD-media scan: process and
  hardware services unrelated to windowing.

The UI module's recovered `UI_HideGameWindow`/`UI_RestoreDesktopDisplay`
exports remain, but their Win32 window-enumeration and desktop-gamma bodies are
gone. They are not called by any recovered screen; the engine owns the only
game window and performs the equivalent transitions through SDL.

## 10. Build-system changes

* `build-mk/sources/client-engine.mk` lists `renderer/platform_glw.c` instead of
  `renderer/platform_wgl.c`.
* The MinGW client already linked `-lSDL2main -lSDL2`; the entry point now uses
  the `WinMain` trampoline that SDL2main provides.
* The CMake client target links `SDL2::SDL2main` in addition to
  `SDL2::SDL2-static` (bundled path) or the resolved SDL2 libraries (system
  path), so MSVC and clang-cl builds also get the Windows entry trampoline.
* No new system library is required for DXGI because the probe resolves
  `CreateDXGIFactory` at run time.

## 11. Behavior parity summary

| Area | Win32 build | SDL build |
| --- | --- | --- |
| Window title | `Call of Duty: United Offensive Multiplayer` | same |
| Window class | `CoD:United Offensive Multiplayer` | `SDL_app` (internal class name) |
| Windowed placement | display origin + `vid_xpos`/`vid_ypos`, clamped | same |
| Desktop fullscreen | desktop-sized popup | `SDL_WINDOW_FULLSCREEN_DESKTOP`, same scaling path |
| Borderless | desktop-sized popup without border | same |
| Exclusive fullscreen | `ChangeDisplaySettingsEx` on selected display | `SDL_SetWindowDisplayMode`/`SDL_WINDOW_FULLSCREEN` |
| Render resolution | mode table | same |
| Color / depth / stencil | PFD values (32/24/8) | same reported values |
| Swap interval | `wglSwapIntervalEXT` | `SDL_GL_SetSwapInterval` |
| Gamma | display ramp + software LUT fallback | same through SDL |
| Gamma on focus loss | desktop ramp restored | same |
| Mouse capture | Raw Input or recentering | relative mode or recentering |
| `in_rawInput` / `in_rawInputActive` | live Raw Input registration | live SDL capture selection |
| Wheel / buttons / keys / text | Win32 messages | SDL events |
| Alt+Enter | `r_fullscreen` toggle + `vid_restart` | same |
| Alt-Tab policy | `win_allowalttab` hotkey | same |
| Window close | quit | quit |
| Splash | Win32 window + bitmap control | SDL window + bitmap surface |
| Error dialogs | `MessageBoxA` | `SDL_ShowMessageBox` |
| Clipboard | Win32 clipboard | SDL clipboard |
| Video memory | DirectDraw + WGL probe | DXGI + SDL GL probe + 256 MB floor |
| Icon | embedded resource 133 | same embedded resource, loaded by SDL |

## 12. Verification

The migration was validated on Windows x86 with the MSVC CMake preset:

1. **Build**: `cmake --build --preset msvc` completes with all client and module
   targets.
2. **Windowed startup**: the game creates a visible `SDL_app` window with the
   retail title, initializes OpenGL 2.1 through SDL, loads the same extension
   set, and presents the main menu at the requested mode (800x600).
3. **Renderer log parity**: vendor, renderer, GL version, extension list,
   `PIXELFORMAT` (32/24/8), mode, texture mode, overbright, and gamma selection
   match the Win32 build on the same host.
4. **Desktop fullscreen**: `r_fullscreen 1` creates a desktop-sized window;
   `r_fullscreen 3` switches the display to 800x600 and restores the desktop on
   exit.
5. **Video restart**: `+vid_restart` performs a complete OpenGL shutdown and
   reinitialization, including a new SDL window and context.
6. **Alt+Enter**: changes `r_fullscreen` from 0 to 1, issues `vid_restart`, and
   re-creates the window in fullscreen.
7. **Window move**: moving the window updates and archives `vid_xpos` and
   `vid_ypos`.
8. **Keyboard/text input**: typing into the in-game console reaches
   `SE_KEY`/`SE_CHAR`, Enter executes the command, and Escape dismisses the
   console.
9. **Mouse paths**: both `in_rawInput 1` (relative mode) and `in_rawInput 0`
   (recentering fallback) activate `in_rawInputActive`.
10. **Clean exit**: closing the game window returns exit status 0 and restores
    the desktop display mode.

The dedicated server and the Linux/macOS client source paths retain their
existing SDL usage; only Windows-specific source in shared files was removed.

## 13. Known differences

These differences are intentional and follow from SDL owning the native window:

* The window class is SDL's `SDL_app`. The title, icon, client size, and
  placement are unchanged. SDL's default decorated style includes a minimize
  button where the original `WS_CAPTION|WS_SYSMENU` window did not.
* The splash window is borderless instead of `WS_POPUP|WS_BORDER|WS_SYSMENU`,
  and it is centered on the primary display.
* SDL minimizes fullscreen windows on focus loss by default. The original
  minimized only the exclusive mode. Desktop fullscreen returns to the same
  display mode when refocused.
* The developer console remains a Win32 control window; its exact chrome is
  unchanged from the Win32 build.
* Log-only differences: the SDL build prints `...using native swap control`
  instead of `...using WGL_EXT_swap_control`, prints an empty
  `WGL_EXTENSIONS`, and omits the WGL pixel-format enumeration diagnostics.
* `win_wndproc` is no longer published because SDL owns the window procedure.
  `win_hinstance` still reports the executable module handle.
* Gamma rejection in true fullscreen is reported as
  `SDL_SetWindowGammaRamp failed.` instead of `SetDeviceGammaRamp failed.`; the
  fallback and resulting image are the same.

## 14. Troubleshooting

* **No window appears**: check `SDL_GetError` output from
  `coduomp_sdl_error_compat` in the renderer warning. The usual causes are an
  unavailable exclusive mode, a display index outside the active display list,
  or failed OpenGL 2.1 compatibility-context creation.
* **Wrong display**: `r_display` is zero-based and must be lower than
  `r_displayCount`; the graphics menu's Monitor selector writes it.
* **Window off-screen**: `vid_xpos`/`vid_ypos` are clamped to the selected
  display, but a stale value from another monitor layout can still be
  surprising; reset them to `0 0`.
* **Mouse does not capture**: verify `in_mouse 1`, `in_rawInput 1`, and that no
  other application holds SDL's relative-mouse grab. `in_rawInputActive 0`
  while in game indicates capture is not active.
* **Gamma not applied**: `r_gammaMode`, `r_ignorehwgamma`, and the host
  display's gamma support determine the provider. The console reports
  `GAMMA: hardware`, `GAMMA: full-frame software`, or `GAMMA: texture fallback`.
