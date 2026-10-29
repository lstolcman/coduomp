# The Windows dedicated server required an unshipped `zlib1.dll`

## Symptom

`coduo_dedicated_recovered.exe` built by the MinGW make target exited
immediately with no output and no log file whenever `zlib1.dll` was not next to
the executable or on `PATH`. Adding the i686 `zlib1.dll` from the toolchain made
it start. The clean and modified retail trees do not ship that DLL, so a
packaged server could not run at all.

## Root cause

`WINDOWS_COMMON_LIBS` linked `-lz`, which the MinGW linker resolves to the
import library `libz.dll.a` when both the import library and the static archive
are installed. The resulting import table was:

```
KERNEL32.dll  WS2_32.dll  msvcrt.dll  zlib1.dll
```

The stock `CoDUOMP.exe` imports no zlib DLL (its import table is only ADVAPI32,
DDRAW, GDI32, KERNEL32, SHELL32, USER32, WINMM, WSOCK32 and `mss32.dll`), so the
compression code is statically linked in the reference build. The recovered
server is also meant to be a single self-contained executable.

## Fix

`build-mk/server.mk` selects the static archive explicitly:

```make
WINDOWS_STATIC_COMPRESSION_LIBS := -Wl,-Bstatic -lz -Wl,-Bdynamic
WINDOWS_COMMON_LIBS ?= -lws2_32 $(WINDOWS_STATIC_COMPRESSION_LIBS)
```

The link order keeps the other system libraries dynamic while forcing zlib to
resolve from `libz.a`. A new `windows-import-check` target inspects the built
executable and fails the build if it imports any unshipped runtime DLL
(`zlib1`, `libgcc*`, `libstdc++*`, `libwinpthread*`, `libssp*`).
`windows-i686` depends on it.

## Validation

- Import table after the change:
  `KERNEL32.dll`, `WS2_32.dll`, `msvcrt.dll` only — no `zlib1.dll`.
- Map start on a tree with `zlib1.dll` renamed away: the server reaches
  `Game Initialization` in ~2 s and stays running, so the statically linked
  zlib reads the compressed paks correctly.
- `windows-import-check` passes.
- The CMake server target links the bundled `libs/` static zlib directly, so it
  does not use `WINDOWS_COMMON_LIBS`; this change only affects the
  Makefile/MinGW server path.

## Files

- `build-mk/server.mk`
- `docs/bugfixes/24_server_zlib_dependency.md`
