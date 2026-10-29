# CoD:UO Dedicated Engine

This repository contains maintained source for the Call of Duty: United Offensive
dedicated-server engine. It covers server-side runtime systems such as
commands, cvars, filesystem access, networking, collision, animation/model
services, scripting support, and game-module host calls.

The source tree is organized as normal buildable C/C++ source:

- `src/server/standalone/`: process and dedicated-platform code.
- `src/server/engine/`: server engine code shared with an embedded listen
  server.
- common subsystems live directly below `src/`.
- `build-mk/server.mk`: compile and link targets for the maintained engine.

## Build

On Linux, run one of these top-level Make targets from the repository root to
build the dedicated-server executable and its matching game module:

```sh
make server64
make server32
```

Choose the architecture you intend to deploy. The 32-bit target requires a
multilib toolchain and matching 32-bit zlib development library. See the
[Linux server build guide](server-building.md#linux-dedicated-server-make)
for requirements, component targets, and output paths under `.workbench/build/`.

CMake exists only for native Windows builds with MSVC or clang-cl. For Windows,
choose the [CMake instructions](server-building.md#windows-with-msvc-or-clang-cl)
or the [MinGW Make instructions](server-building.md#windows-dedicated-server-with-mingw-make).
Linux builds use Make.

Archive and shared-link checks are documented separately under
[Compile Checks](server-building.md#compile-checks). They are developer
validation targets; use the full server targets above for a runnable build.
