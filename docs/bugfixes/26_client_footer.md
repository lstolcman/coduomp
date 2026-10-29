# The main-menu footer did not identify the build

## Symptom

The main menu footer showed only `1.51`. A screenshot of the game could not
tell which build produced it, which made it impossible to confirm that a
reported problem came from the current sources rather than an older
executable/module pair.

## Root cause

`ui_compat_brand_main_menu_version()` replaced the retail
`background_version_display` text with `CODUOMP_DISPLAY_LABEL`
(`"Open CoD:UO 1.51"`), and the footer item itself only carries
`shortversion`. Nothing in the UI module referenced the build identity.

## Fix

`src/client/common/client_branding.h` gained:

```c
#define CODUOMP_DISPLAY_FOOTER \
    CODUOMP_DISPLAY_VERSION " (" CODUOMP_GIT_REVISION " " \
    CODUOMP_DISPLAY_BUILD_DATE ")"
```

`ui_compat_brand_main_menu_version()` now assigns it to the footer item, so the
footer reads:

```text
1.51 (24c0bbdb-dirty Oct 28 2026)
```

The numeric `shortversion` cvar and everything derived from it stay at `1.51`
for network compatibility; only the displayed text changes. The identity comes
from the same automatic build definition used by bug 25 (CMake
`coduomp_apply_build_identity()`, `build-mk/ui.mk` for the MinGW modules), so no
manual `-D` is needed and a modified tree keeps the `-dirty` marker.

## Validation

- `cmake --build --preset msvc --target uo_ui_mp_x86 --parallel 6` succeeds;
  the DLL contains `1.51 (24c0bbd Oct 28 2026)` (the CMake path has no git
  client on `PATH`, so it uses the `.git` refs fallback without the `-dirty`
  marker).
- `make -f build-mk/ui.mk win32-abi-link` succeeds; the deployed i686 DLL
  contains `1.51 (24c0bbdb-dirty Oct 28 2026)`.
- Runtime: the client was started from `coduomp_tree` with a temporary
  `fs_homepath`, and the main menu shows the footer in the screenshot
  `docs/bugfixes/26_client_footer_identity/footer_menu.png`.

## Files

- `src/client/common/client_branding.h`
- `src/client/ui/module/ui_compat_graphics_menu.c`
- `docs/bugfixes/26_client_footer_identity/footer_menu.png`
- `docs/bugfixes/26_client_footer.md`
