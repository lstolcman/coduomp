# Building

Linux dedicated-server and game-module builds use the maintained Make targets.
Use `make server32` or `make server64` to build both components for the selected
architecture. Windows builds with MinGW also use Make. CMake exists only for
native Windows builds with MSVC or clang-cl; the top-level CMake project and
presets do not support Linux or macOS builds.

The public tree contains two independently useful target families:

- `build-mk/server.mk` builds the standalone server from
  `src/server/standalone/`, `src/server/engine/`, and the common engine source.
- `build-mk/game.mk` builds the server game module from `src/server/game/` and
  its shared BG/qcommon/math dependencies.

Top-level Make targets compile in parallel by default, with one job per
detected processor. `make JOBS=N <target>` selects a budget and `JOBS=1`
builds serially. An existing `make -jN` jobserver takes precedence over
`JOBS` and is shared by recursive builds. Direct `make -f build-mk/...`
invocations need an explicit `-jN` to compile in parallel.

## CD-key Authorization

On `master`, dedicated and listen servers skip external CD-key authorization
by default. Dedicated-server builds can enable authorization with `AUTH=1`:

```sh
make server64 AUTH=1
make server32 AUTH=1
```

When invoking `build-mk/server.mk` directly, set
`CODUO_DISABLE_SERVER_AUTH=0` to enable authorization. The `stock` branch keeps
its original authorization defaults.

## Requirements

- `make`
- C and C++ compilers such as `cc`, `c++`, `clang`, or `gcc`
- `ar`
- For Windows cross-builds, an architecture-matched MinGW compiler,
  `i686-w64-mingw32-gcc` or `x86_64-w64-mingw32-gcc`, and its matching `g++`
- MinGW-compatible zlib headers and library for the Windows engine
- For native Windows builds, CMake 4.4+ and MSVC 19.39+ or clang-cl 22+

## Compile Checks

```sh
make -f build-mk/game.mk check CODUO_FP_FAITHFUL=relaxed
make -f build-mk/server.mk objects CODUO_FP_FAITHFUL=relaxed
```

The game-module check compiles source into a static archive. The
dedicated-engine check compiles maintained engine code into a static archive.

## Shared Module Builds

For server deployment, build the 32-bit Linux module:

```sh
make server-game32
```

Output:

```text
.workbench/build/game/linux-i386/game.mp.uo.i386.so
```

This target uses `-m32`, so it requires a Linux toolchain with 32-bit compiler
and library support installed.

For local shared-object experiments using the host toolchain:

```sh
make server-game64
```

Output:

```text
.workbench/build/game/linux-x86_64/game.mp.uo.x86_64.so
```

For a local dedicated-engine shared-object link check, use the target Makefile:

```sh
make -f build-mk/server.mk shared-check-link CODUO_FP_FAITHFUL=relaxed
```

For a native dedicated-server executable:

```sh
make server-engine64
```

Output:

```text
.workbench/build/server/linux-x86_64-noauth/coduo_lnxded_recovered
```

## Windows with MSVC or clang-cl

The same CMake target builds a dedicated engine and matching game DLL for
all supported Windows compiler configurations:

| Preset | Architecture | Compiler |
| --- | --- | --- |
| `msvc` | x86 (32-bit) | MSVC |
| `msvc-x64` | x86-64 | MSVC |
| `clang-cl-x64` | x86-64 | clang-cl 22+ |

Each preset builds the client, its three modules, and the dedicated server by
default. Select any row's preset for both commands:

```sh
cmake --preset msvc
cmake --build --preset msvc --parallel 2
```

The Visual Studio MSVC presets apply this budget to compiler processes within
each project and build project references sequentially. `--parallel` without
a number uses the processor count. See the [client build guide](client-building.md#windows-with-clang-cl-or-msvc)
for serial builds, PDB coordination, and other generators/toolsets.

For a server-only build, disable the client and its optional modules. The
server still builds its required game DLL; only zlib is fetched as a dependency:

```sh
cmake --preset msvc -DCODUOMP_BUILD_CLIENT=OFF -DCODUOMP_BUILD_MODULES=OFF
cmake --build --preset msvc --target coduo_dedicated_recovered --parallel 2
```

Outputs are `.workbench/build/<preset>/bin/Release/coduo_dedicated_recovered.exe`
and `bin/Release/uo/uo_game_mp_x86.dll` in that same build directory. The DLL
filename is the same on x86 and x64; the engine and DLL must have matching
architectures. Selecting the server target builds both files.

`BUNDLED_LIBS=OFF` with `CODUOMP_DEP_PREFIX` selects architecture-matched MSVC
libraries instead. A server-only configuration then resolves only zlib.
`CODUOMP_BUILD_SERVER=OFF` retains a client/module-only configuration.
`CODUOMP_DISABLE_SERVER_AUTH=ON` disables external CD-key authorization.
Its default preserves the branch's Makefile authorization setting. The Windows
CMake server uses the disabled PunkBuster provider; the recovered dynamic
PunkBuster loader currently requires POSIX services.

Both engine build systems reserve 8 MiB of stack with a 4 KiB initial commit.
MSVC's binary64 `long double` limits and clang-cl's explicit extended-arithmetic
profile also apply to the server; see [client build limits](client-building.md#windows-with-clang-cl-or-msvc).

## Windows Dedicated Server

The top-level Windows targets build the engine and matching game DLL.
`server-windows` builds both 32-bit and 64-bit products:

```sh
make server-windows
make server-windows-i686
make server-windows-x86_64
make server-windows-i386
```

Outputs:

```text
.workbench/build/server/windows-i386/coduo_dedicated_recovered.exe
.workbench/build/game/windows-i386/uo_game_mp_x86.dll
.workbench/build/server/windows-i686/coduo_dedicated_recovered.exe
.workbench/build/game/windows-i686/uo_game_mp_x86.dll
.workbench/build/server/windows-x86_64/coduo_dedicated_recovered.exe
.workbench/build/game/windows-x86_64/uo_game_mp_x86.dll
```

The i386 and i686 targets use the 32-bit MinGW ABI; x86-64 uses PE32+.
All targets use `-O0 -mfpmath=387
-fexcess-precision=fast` in both engine and module C/C++ compilation. The
Windows engine establishes the retail-compatible 53-bit x87 precision mode at
startup and restores it after loading the game DLL. The distinction between
the targets is the generated instruction baseline: `-march=i386` or
`-march=i686`.

The engine needs MinGW zlib. If it is not installed in the compiler's default
search path, point `MINGW32_DEP_PREFIX` or `MINGW64_DEP_PREFIX` at a prefix containing `include/zlib.h`
and `lib/libz.a`:

```sh
make server-windows-i686 MINGW32_DEP_PREFIX=/opt/mingw-i686
make server-windows-x86_64 MINGW64_DEP_PREFIX=/opt/mingw-x86_64
```

Override the compiler names when necessary:

```sh
make server-windows-i386 \
  MINGW32_CC=i386-mingw32-gcc \
  MINGW32_CXX=i386-mingw32-g++ \
  WINDOWS_DEP_PREFIX=/opt/mingw-i386
```

The generated DLL exports only `dllEntry` and `vmMain`, matching the retail
module. Copy the DLL into the game's `uo` directory when testing.
Place the engine executable beside the `main/` and `uo/` game-data directories,
or pass an explicit `+set fs_basepath`. The engine loads
`uo/uo_game_mp_x86.dll` through its Win32 loader path.

## 32-bit Module Notes

The original Linux game module is i386. Building the 32-bit shared module
requires a Linux toolchain with i386 support.

## Build Outputs

Build products are intentionally ignored. Clean them with ordinary non-forced
file deletion or by removing the generated build directories manually.
