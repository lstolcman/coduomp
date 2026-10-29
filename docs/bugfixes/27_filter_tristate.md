# The server-browser filters had no "All" choice

## Symptom

The Filter Servers popup offered only Yes/No for View Empty, View Full, With
Password, Without Password, Pure Servers only and Dedicated Only. The rows for
Mods, Friendly Fire, Killcam, PunkBuster, Jeeps and Tanks already have an
explicit `All`, so the same popup mixed two different models and a player could
not neutralise the six Yes/No filters.

## Root cause

`ui_mp/filter.menu` declares those six items as `ITEM_TYPE_YESNO`. A yes/no item
can only write `0` or `1`, and `UI_BuildServerDisplayList()` treated any
non-zero value as a condition for Pure and Dedicated:

```c
} else if (ui_browserShowPure.integer && pure == 0) {
} else if (ui_browserShowDedicated.integer) {
```

so `-1` would have meant "require", not "no condition".

## Fix

The recovered client converts the six parsed rows to selectors at menu load,
without editing the proprietary menu asset (and therefore without changing any
pak checksum):

- new `src/client/ui/browser/ui_compat_filter_menu.c` finds the six rows by
  cvar in `filter_popmenu` and installs
  `@MENU_ALL -1 / @MENU_NO 0 / @MENU_YES 1` in the parsed-menu memory pool;
- `UI_BuildServerDisplayList()` now treats `-1` as "no condition" for Pure and
  Dedicated (`== 1`); Empty, Full, Password and NoPassword already used
  `!value`, which is true only for `0`;
- `ui_compat_extend_filter_menu()` runs from `UI_Init()` after the stock menus
  are loaded.

The existing Mod/FF/Killcam/PB/Jeeps/Tanks rows keep their `-1` = All handling,
so the whole popup now behaves the same way, and a pre-existing `0`/`1` value
from an old config still selects No/Yes.

## Validation

- CMake (`uo_ui_mp_x86`) and MinGW (`make -f build-mk/ui.mk win32-abi-link`)
  both compile without warnings.
- Runtime: the stock `ui_mp/filter.menu` is loaded unchanged, and the popup now
  shows all three states. A temporary test pak opened the popup from the main
  menu (synthetic input is not delivered to the game window in this
  environment) with `ui_browserShowEmpty -1`, `ui_browserShowFull 0`,
  `ui_browserShowPassword 1`, `ui_browserShowNoPassword -1`,
  `ui_browserShowPure 1`, `ui_browserShowDedicated -1`. The screenshot
  `filter_popup_crop.png` shows All / No / Yes / All / Yes / All, proving that
  each value maps to its label.
- The test pak was removed after the capture.

## Files

- `src/client/ui/browser/ui_compat_filter_menu.c` (new)
- `src/client/ui/browser/ui_build_server_display_list.c`
- `src/client/ui/module/ui_init.c`
- `src/client/ui/module/ui_functions.h`
- `build-mk/sources/ui.mk`
- `docs/bugfixes/27_filter_tristate.md`
- `docs/bugfixes/27_filter_tristate/filter_popup_crop.png`
