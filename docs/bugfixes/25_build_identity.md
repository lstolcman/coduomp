# The CMake build reported no source revision

## Symptom

Bug 06 embedded `git describe --always --dirty` in the **server** banner, but
only for the Makefile targets. A CMake/MSVC build had no revision at all, and
the client banner never named one:

- `cmake --preset msvc` emitted no revision and defined no
  `CODUOMP_GIT_REVISION` for any target;
- the client startup banner printed only the product, version and date;
- an incremental Makefile build kept reporting the revision of the last time
  `com_init_runtime.o` had been compiled, so the banner went stale after a
  commit even though the executable had been relinked.

## Root cause

The revision was resolved only inside `build-mk/server.mk`, and nothing made
the compiled-in identity depend on the git metadata. The object that stores the
banner text was rebuilt only when its source changed, so moving `HEAD` or
rewriting the index did not trigger a recompile.

## Fix

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

## Validation

- `cmake --preset msvc` prints `-- Build revision: <rev>`; the generated
  projects contain `CODUOMP_GIT_REVISION="<rev>"`.
- `cmake --build --preset msvc --parallel 6` succeeds, and
  `strings .workbench/build/msvc/bin/Release/CoDUOMP.exe` and
  `.../coduo_dedicated_recovered.exe` contain `<rev>`.
- `make -f build-mk/ui.mk -n win32-abi-link` and the MinGW client compile rule
  both expand to `-DCODUOMP_GIT_REVISION='"<rev>-dirty"'` when the tree is dirty
  (a clean tree drops the marker). A full MinGW client link was not run here
  because this host has no MinGW dependency prefix for SDL2/libjpeg/libcurl;
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

## Files

- `CMakeLists.txt`
- `cmake/CoduompBuildIdentity.cmake` (new)
- `cmake/CoduompTargets.cmake`
- `src/client/common/client_branding.h`
- `src/client/engine/com_startup.c`
- `build-mk/client-engine-mingw.mk`
- `build-mk/ui.mk`
- `build-mk/server.mk`
- `docs/bugfixes/25_build_identity.md`
