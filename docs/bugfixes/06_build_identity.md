# Build identity in every build and in the main-menu footer

## The dedicated server reported a stale build identity

### Symptom

The dedicated server's first log line always claimed the same build:

```
CoD:UO Recovered Dedicated Engine 1.51 (windows-i386) build Jul 01 2026
```

The date was a literal in `com_init_runtime.c`, so it did not describe the
binary that was actually running, and there was no way to tell which source
revision produced a build when reading a server log or a user's report.

### Investigation

- `COM_INIT_BUILD_DATE` was `#define COM_INIT_BUILD_DATE "Jul 01 2026"` and
  `Com_Init` printed it before the filesystem started. Nothing else in the
  banner identified the build.
- The dedicated server is built by `build-mk/server.mk` for Linux and MinGW
  Windows targets. The root `Makefile` already computes `SOURCE_COMMIT` for
  package names, but sub-makes only inherit command-line variables, so
  `server.mk` could not see it.
- The client has its own display identity in
  `src/client/common/client_branding.h` using `__DATE__` and `__TIME__`. The
  dedicated server had no equivalent.
- `Sys_PrintBinVersion`, used only by `--version`, prints the retail Linux
  banner (`Linux Quake3 Dedicated Server [Feb 10 2005 15:44:04]`). That string
  identifies the stock binary and is intentionally left unchanged.

### Fix

- New `src/server/standalone/core_runtime/coduomp_build_info.h` supplies
  `CODUOMP_BUILD_DATE` (`__DATE__`), `CODUOMP_BUILD_TIME` (`__TIME__`) and a
  fallback `CODUOMP_GIT_REVISION` of `"unknown"`.
- `build-mk/server.mk` defines `CODUOMP_GIT_REVISION` with
  `git describe --always --dirty` when git metadata is available. The
  `-dirty` suffix marks a build whose working tree does not match the recorded
  commit; a release tarball without `.git` reports `unknown`.
- The startup line now reads:

  ```
  CoD:UO Recovered Dedicated Engine 1.51 (linux-x86_64) build Oct 28 2026 02:19:26 git 3e1c6e9a
  ```

  The date and time come from the translation unit's compilation, like the
  client's `Com_GetBuildVersion`, and the revision is embedded at build time.

### Validation

- Built `server-engine64` on a Linux build host and ran it briefly; it reported
  `build Oct 28 2026 02:19:26 git 3e1c6e9a-dirty`.
- Built `server-windows-i686` with the same define; the resulting
  `coduo_dedicated_recovered.exe` contains the revision and compile date
  strings and no longer contains `Jul 01 2026`.
- The client targets are untouched; their banner stays
  `Open CoD:UO 1.51 build <date>`.
- `make policy-check` passes.

### Notes

`__DATE__` and `__TIME__` describe when the banner translation unit was last
compiled. An incremental build that does not recompile `com_init_runtime.c`
keeps the earlier timestamp, which is the same behavior as the client banner.
`-dirty` is intentional: a server built from a modified tree must not claim to
be the recorded commit.

### Files

- `src/server/standalone/core_runtime/coduomp_build_info.h`
- `src/server/standalone/core_runtime/com_init_runtime.c`
- `build-mk/server.mk`
- `docs/bugfixes/06_build_identity.md`

## The CMake build reported no source revision

### Symptom

The server banner now embedded `git describe --always --dirty`, but only for
the Makefile targets. A CMake/MSVC build had no revision at all, and
the client banner never named one:

- `cmake --preset msvc` emitted no revision and defined no
  `CODUOMP_GIT_REVISION` for any target;
- the client startup banner printed only the product, version and date;
- an incremental Makefile build kept reporting the revision of the last time
  `com_init_runtime.o` had been compiled, so the banner went stale after a
  commit even though the executable had been relinked.

### Root cause

The revision was resolved only inside `build-mk/server.mk`, and nothing made
the compiled-in identity depend on the git metadata. The object that stores the
banner text was rebuilt only when its source changed, so moving `HEAD` or
rewriting the index did not trigger a recompile.

### Fix

New `cmake/CoduompBuildIdentity.cmake`, included by `CMakeLists.txt`:

- resolves `git describe --always --dirty` with `find_program(git)` and
  `execute_process()` when a git client is available (Linux and the usual
  Windows developer shell);
- otherwise reads the revision from `.git` directly — `HEAD`, the loose ref,
  or `packed-refs` — so a Windows host with Git installed but `git.exe` not on
  `PATH` still reports the checked-out commit (7 characters, without the
  `-dirty` marker, which needs the git client);
- reports `unknown` for an exported source archive;
- registers `.git/HEAD` and `.git/index` with `CMAKE_CONFIGURE_DEPENDS`, so
  MSBuild/ZERO_CHECK re-runs configure after a commit and the affected
  translation units recompile.

Nothing is passed on the command line: `cmake --preset msvc` alone produces a
correct identity.

The resolved value is stored in `CODUOMP_BUILD_GIT_REVISION`, not
`CODUOMP_GIT_REVISION`. `cmake/CoduompSources.cmake` evaluates the `build-mk`
manifests (which define a Makefile variable of the latter name) into the CMake
scope and `coduomp_manifest_reset()` unsets every variable they create. Using
the Makefile name directly produced an empty define
(`CODUOMP_GIT_REVISION=""`) in every generated project; the distinct name
keeps the two apart.

`coduomp_apply_build_identity(<target>)` adds the definition to `CoDUOMP`,
`uo_ui_mp_x86` and the CMake dedicated server (`coduo_dedicated_recovered`), so
every CMake product reports the same revision.
`src/client/common/client_branding.h` keeps a default of `"unknown"` for builds
that do not supply it, and the startup banner now prints it:

```text
Open CoD:UO 1.51 build <platform> <date> git <revision>
```

The Makefile side gained the same rules:

- `build-mk/client-engine-mingw.mk` and `build-mk/ui.mk` define
  `CODUOMP_GIT_REVISION` automatically (no manual `-D`), before the flags that
  expand it;
- `build-mk/client-engine-mingw.mk` recompiles `com_startup.c.o` when
  `.git/HEAD` or `.git/index` changes;
- `build-mk/server.mk` recompiles `com_init_runtime.o` for the same files, so a
  relink can no longer keep an older revision in the banner.

### Validation

- `cmake --preset msvc` prints `-- Build revision: <rev>`; the generated
  projects contain `CODUOMP_GIT_REVISION="<rev>"`.
- `cmake --build --preset msvc --parallel 6` succeeds, and
  `strings .workbench/build/msvc/bin/Release/CoDUOMP.exe` and
  `.../coduo_dedicated_recovered.exe` contain `<rev>`.
- `make -f build-mk/ui.mk -n win32-abi-link` and the MinGW client compile rule
  both expand to `-DCODUOMP_GIT_REVISION='"<rev>-dirty"'` when the tree is dirty
  (a clean tree drops the marker). A full MinGW client link was not run here
  because the build host has no MinGW dependency prefix for
  SDL2/libjpeg/libcurl;
  the compile command is verified from the Makefile expansion.
- `make -f build-mk/server.mk windows-i686` recompiles `com_init_runtime.o`
  with the same definition after the commit, and the executable embeds the
  revision; `windows-import-check` still passes.
- The CMake dedicated server started with `+set fs_homepath <temp>` prints the
  revision in its banner, so the server target added by the same branch reports
  the identity too.
- After a commit in the same build tree, `cmake --build` re-runs configure and
  prints the new revision (the earlier stale `c40b62f` value was observed
  before this change and no longer reproduces).

### Files

- `CMakeLists.txt`
- `cmake/CoduompBuildIdentity.cmake` (new)
- `cmake/CoduompTargets.cmake`
- `src/client/common/client_branding.h`
- `src/client/engine/com_startup.c`
- `build-mk/client-engine-mingw.mk`
- `build-mk/ui.mk`
- `build-mk/server.mk`
- `docs/bugfixes/06_build_identity.md`

## The main-menu footer did not identify the build

### Symptom

The main menu footer showed only `1.51`. A screenshot of the game could not
tell which build produced it, which made it impossible to confirm that a
reported problem came from the current sources rather than an older
executable/module pair.

### Root cause

`ui_compat_brand_main_menu_version()` replaced the retail
`background_version_display` text with `CODUOMP_DISPLAY_LABEL`
(`"Open CoD:UO 1.51"`), and the footer item itself only carries
`shortversion`. Nothing in the UI module referenced the build identity.

### Fix

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
from the same automatic build definition used by the build identity
section (CMake `coduomp_apply_build_identity()`, `build-mk/ui.mk` for the
MinGW modules), so no manual `-D` is needed and a modified tree keeps the
`-dirty` marker.

### Validation

- `cmake --build --preset msvc --target uo_ui_mp_x86 --parallel 6` succeeds;
  the DLL contains `1.51 (24c0bbd Oct 28 2026)` (the CMake path has no git
  client on `PATH`, so it uses the `.git` refs fallback without the `-dirty`
  marker).
- `make -f build-mk/ui.mk win32-abi-link` succeeds; the built i686 DLL
  contains `1.51 (24c0bbdb-dirty Oct 28 2026)`.
- Runtime: the client was started from the build tree with a temporary
  `fs_homepath`, and the main menu shows the footer in the screenshot
  `docs/bugfixes/06_build_identity/footer_menu.png`.

### Files

- `src/client/common/client_branding.h`
- `src/client/ui/module/ui_compat_graphics_menu.c`
- `docs/bugfixes/06_build_identity/footer_menu.png`
- `docs/bugfixes/06_build_identity.md`
