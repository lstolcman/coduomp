# Build documentation fixes for parallel builds, hunk memory and preset names

## CMake examples omitted `--parallel`

### Symptom

The Linux and Linux-Clang build examples in `README.md` and
`docs/server-building.md` were written as a bare `cmake --build --preset ...`.
Copying them builds with one job, so a full engine plus game-module build takes
many times longer than the machine allows, even though every supported host has
multiple cores.

### Root cause

The native Linux presets were added with the shortest possible command pair, and
the `--parallel` flag was only added later to the Make targets and to the MSVC
examples. Nothing enforced the flag across the documentation, so the two Linux
examples and the README quick-start kept the serial form. The explicit
`.workbench/build/linux-cmake` example already used `--parallel`.

### Fix

Every `cmake --build` example for every platform now carries a parallel flag:

- `README.md`: `cmake --build --preset linux-gcc-64bit --parallel`.
- `docs/server-building.md`: `cmake --build --preset linux-gcc-64bit --parallel` and
  `cmake --build --preset linux-clang-64bit --parallel`.
- The Windows examples keep `--parallel 2`, which bounds concurrent MSVC
  compiler processes per project to limit peak memory while still building in
  parallel.

`--parallel` without a number uses one job per logical processor, which is the
right default for a developer workstation or a build host. Automated builds
use `--parallel 8` explicitly.

### Validation

- `grep -rn 'cmake --build' README.md docs/*.md` reports a parallel flag on
  every build command; the remaining matches are prose.
- Rebuilt both native Linux presets with `--parallel 8`:
  `cmake --build --preset linux-gcc-64bit --parallel 8` and
  `cmake --build --preset linux-clang-64bit --parallel 8` produced
  `coduo_dedicated_recovered` and `uo/game.mp.uo.x86_64.so`.

## Server test command lowered the hunk below its default

### Symptom

The "Running the CMake-built server" example in `docs/server-building.md`
started the server with `+set com_hunkmegs 64`. A reader copying the command got
64 MiB of hunk memory instead of the 128 MiB the dedicated server allocates by
default, which is the opposite of what a first smoke test needs: the lower
value only increases the risk of an out-of-memory abort on a large map.

### Root cause

`Com_InitHunkMemory` reads `com_hunkMegs` with
`HUNK_DEFAULT_DEDICATED_MEGABYTES`, which is `"128"` for a dedicated server
(`HUNK_MINIMUM_MEGABYTES_DEDICATED` is 1). The example predates that default
being confirmed, and the cvar name was also written in lower case, so it was
easy to read as a required setting rather than an override.

### Fix

The override is removed from the sample command. The remaining arguments are
enough for a first start, and the server archives `com_hunkMegs "128"` into
`fs_homepath/uo/uoconfig_mp.cfg` on its own.

### Validation

Both native Linux presets were built and started against the unmodified
stock tree with a temporary `fs_homepath`, `sv_pure 1` and `+map mp_carentan`:

| Preset | Result | Archived value |
| --- | --- | --- |
| `linux-gcc-64bit` (GCC) | reaches `Game Initialization` | `seta com_hunkMegs "128"` |
| `linux-clang-64bit` (Clang) | reaches `Game Initialization` | `seta com_hunkMegs "128"` |

The source is shared by both presets, so the same
`HUNK_DEFAULT_DEDICATED_MEGABYTES` applies; the runtime check confirms the
archived value in each build.

### Files

- `docs/server-building.md`
- `docs/bugfixes/31_build_docs.md`

## Ambiguous CMake preset names

### Symptom

The checked-in presets were `msvc`, `msvc-x64`, `clang-cl-x64`, `linux` and
`linux-clang`. The names did not say which operating system, compiler or word
size a preset produces:

- `linux` and `linux-clang` read as "the Linux build" and "the Clang build",
  not as "the 64-bit GCC Linux build" and "the 64-bit Clang Linux build",
  although both are 64-bit and the 32-bit builds live in the Makefiles.
- `msvc` read as "the MSVC build" while it is specifically the 32-bit x86
  build; `msvc-x64` looked like its sibling rather than the 64-bit port.
- `clang-cl-x64` mixed a toolset name with a bit-width suffix that used a
  different spelling (`x64`) than the other presets.

A reader could not tell from the name whether a preset was the recommended
build, and nothing in the docs stated which compiler to prefer on Linux.

### Fix

Every configure and build preset follows `<os>-<compiler>-<bits>`:

| Old name | New name | Purpose |
| --- | --- | --- |
| `msvc` | `windows-msvc-32bit` | 32-bit MSVC, `/arch:IA32`, recommended |
| `msvc-x64` | `windows-msvc-64bit` | 64-bit MSVC |
| `clang-cl-x64` | `windows-clangcl-64bit` | 64-bit clang-cl |
| `linux` | `linux-gcc-64bit` | 64-bit GCC server, recommended |
| `linux-clang` | `linux-clang-64bit` | 64-bit Clang server, alternative |

Build directories follow the preset name, so the staged outputs moved from
`.workbench/build/linux/` to `.workbench/build/linux-gcc-64bit/` and likewise
for the other presets. Nothing else changes: the same generators,
architectures, toolsets and cache variables are inherited, and `binaryDir`
still derives from `${presetName}`.

Documentation now states the preference explicitly instead of listing builds
as equals:

- Linux: GCC is the recommended compiler because it can emit the original
  `-mfpmath=387` model. The Clang page is titled "Linux Clang build
  (alternative)" and says collision and patch-winding geometry can round
  differently, so a Clang server is for compile, protocol and asset testing.
- Windows: the 32-bit MSVC preset is recommended because `/arch:IA32` keeps
  compiler-generated arithmetic on the x87 unit and the module matches the
  shipped module's instruction baseline. The x64 presets remain documented
  alternatives with their floating-point limits.

`CMakeLists.txt`, `README.md`, `docs/client-building.md`,
`docs/server-building.md` and the configure-time messages in
`cmake/CoduompPlatform.cmake` use the new names. The historical
build-identity report keeps the names that were current when its evidence was
collected.

### Validation

- `cmake --preset linux-gcc-64bit` + `cmake --build --preset linux-gcc-64bit
  --parallel 8`: built `coduo_dedicated_recovered` and
  `uo/game.mp.uo.x86_64.so` under `.workbench/build/linux-gcc-64bit/`.
- `cmake --preset linux-clang-64bit` + `cmake --build --preset
  linux-clang-64bit --parallel 8`: same two targets under
  `.workbench/build/linux-clang-64bit/`, with `CODUO_FP_FAITHFUL=relaxed`
  inherited from the GCC preset.
- `cmake --preset windows-msvc-32bit` + `cmake --build --preset
  windows-msvc-32bit --parallel 8`: reconfigured from scratch and rebuilt
  `CoDUOMP.exe`, the three modules and `coduo_dedicated_recovered.exe` under
  `.workbench/build/windows-msvc-32bit/`.
- `cmake --list-presets=build` reports exactly the five new names; on Linux
  `cmake --list-presets` lists the two native presets supported by the host.
