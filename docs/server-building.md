# Building

The public tree contains two independently useful target families:

- `build-mk/server.mk` builds the standalone server from
  `src/server/standalone/`, `src/server/engine/`, and the common engine source.
- `build-mk/game.mk` builds the server game module from `src/server/game/` and
  its shared BG/qcommon/math dependencies.

`CMakeLists.txt` builds the same dedicated-server executable with the Visual
Studio toolchain on Windows and with GCC or Clang on Linux; see the platform
sections below.

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
- For Windows cross-builds, a 32-bit MinGW compiler such as
  `i686-w64-mingw32-gcc` plus its matching `g++`
- MinGW-compatible zlib headers and library for the Windows engine
- For the native Windows build, CMake 3.21 or newer and the Visual Studio C++
  build tools

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
make game32
```

Output:

```text
build/game/linux-i386/game.mp.uo.i386.so
```

This target uses `-m32`, so it requires a Linux toolchain with 32-bit compiler
and library support installed.

For local shared-object experiments using the host toolchain:

```sh
make game64
```

Output:

```text
build/game/linux-x86_64/game.mp.uo.x86_64.so
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
.workbench/build/server/linux-x86_64-noauth/coduo_lnxded
```

## Native Linux build (CMake)

`CMakeLists.txt` also builds the dedicated server natively with GCC or Clang.
A clean Ubuntu installation needs CMake 3.21 or newer and the compiler
toolchain; the bundled zlib in `libs/` is used by default, so no development
packages are required:

```sh
sudo apt-get update
sudo apt-get install -y cmake build-essential
```

Create a build directory, configure, and build the 64-bit server:

```sh
mkdir build && cd build
cmake -DBUNDLED_LIBS=YES ..
cmake --build . --target coduo_dedicated --parallel 4
```

The Unix Makefiles generator is single-configuration: the build type is chosen
at configure time with `CMAKE_BUILD_TYPE`, which defaults to `Release` here.
Build a debug binary with `cmake -DCMAKE_BUILD_TYPE=Debug ..`; `--config` only
selects a configuration with multi-configuration generators such as Visual
Studio. Both configurations use the `build-mk/server.mk` flags and x87 policy, so
Debug differs only by its debug information. Change `--parallel 4` to suit the
host.

Outputs:

```text
build/bin/Release/coduo_lnxded
build/bin/Release/uo/game.mp.uo.x86_64.so
```

With `-DCMAKE_BUILD_TYPE=Debug` the same files are staged under
`build/bin/Debug`.

The target is named `coduo_dedicated`; on Linux it produces `coduo_lnxded`, and
the same target produces `coduo_dedicated.exe` on Windows.

The server target depends on the game module, so one build produces both. Put
the module in the `uo/` directory of a filesystem search path before starting a
map; the engine loads it there through `dlopen()`.

The first configure downloads the pinned zlib archive into `libs/` and later
configures reuse it; pass `-DBUNDLED_LIBS=OFF` to link a system zlib
(`zlib1g-dev`) instead. The native configuration builds the dedicated server and
its game module only; the client executable and its client modules come from the
Visual Studio configuration. If the compiler or `make` is missing, configure
stops and names the package to install.

The native configuration builds the architecture the compiler targets, which is
x86-64 on a standard host. Use the Makefile `server32` target for the i386
engine and module. Both architectures speak the same network protocol, so
clients running the original 32-bit executable connect to the 64-bit server
without any change.

### Running the server

No configuration files are needed for a first test: these arguments set
everything the server needs, and it writes its own configuration into
`fs_homepath`.

```sh
cd build/bin/Release

# Directory that contains the game data, with main/ and uo/ directly below it.
GAME_DATA=/path/to/call-of-duty-uo

# The engine loads the game module from uo/ in fs_homepath (default ~/.callofduty).
mkdir -p ~/.callofduty/uo
ln -sfn "$PWD/uo/game.mp.uo.x86_64.so" ~/.callofduty/uo/game.mp.uo.x86_64.so

./coduo_lnxded \
  +set dedicated 2 \
  +set net_ip "localhost" \
  +set net_port 28960 \
  +set sv_hostname "test" \
  +set sv_pure 0 \
  +set fs_basepath "$GAME_DATA" \
  +map mp_carentan
```

`fs_basepath` is the directory that contains `main/` and `uo/`. The default
`fs_homepath` is `~/.callofduty`, which receives configuration files and is
also searched for the module, so the game-data directory is left untouched. On
a case-sensitive filesystem the game-data directories must be named `main` and
`uo` in lower case. `mp_carentan` ships with the game; replace it with another
map shipped in the game data. Stop the server with Ctrl+C.

The example binds `net_ip` to `localhost`, so only players on the same machine
can connect. Use `+set net_ip 0.0.0.0` to accept players from the network; see
[Network ports](#network-ports).

## Network ports

The engine opens one UDP socket for the game port and does not listen on TCP.
Give the server host a fixed LAN address, then forward the game port to that
address on the router and allow it through the host firewall:

| Direction | Protocol | Port | Purpose |
| --- | --- | --- | --- |
| Inbound | UDP | `28960` (`net_port`) | Gameplay, server queries, in-game downloads |
| Outbound | UDP | `20610` | Master-server heartbeat for the in-game server list |
| Outbound | UDP | `20600` | CD-key authorization when it is enabled |

For example, with `ufw`:

```sh
sudo ufw allow 28960/udp
```

Use the equivalent rule for nftables or firewalld. Start the server with
`+set net_ip 0.0.0.0` and, if the forwarded port differs from the default,
`+set net_port <port>`. Players then connect to `<public-ip>:28960`.

If port `28960` is already in use, the engine tries the next nine ports
(`28961` through `28969`), updates `net_port`, and prints the port it bound
(`Opening IP socket: ...:<port>`). Forward or allow that port instead. Each
additional server on the same host needs its own port and forwarding rule.

Only the game port needs an inbound rule. The master heartbeat and CD-key
authorization are outbound connections from the server, so replies arrive on
the existing UDP mapping. If outbound UDP is filtered, direct connections still
work, but the server does not appear in the in-game list and authorization
cannot complete.

TCP is not used for gameplay. Setting `sv_wwwDownload 1` with `sv_wwwBaseURL`
makes clients download mod files from that HTTP server over TCP, so open the web
server's port rather than adding one on the game host.

## Native Windows build (MSVC)

`CMakeLists.txt` builds the dedicated server with the Visual Studio toolchain
together with the client targets. It uses the bundled `libs/` tree
described in `client-building.md`, so no MinGW toolchain or external zlib
installation is required.

```sh
mkdir build && cd build
cmake -A Win32 -DBUNDLED_LIBS=YES ..
cmake --build . --config Debug --parallel 4
```

CMake selects the newest installed Visual Studio generator; add `-G` with a
generator from `cmake --help` to build with another installed version.

`--parallel 4` compiles four source files at a time; projects build one after
another, so the number is the budget for the whole build. Change it to suit the
host; `--parallel` without a number uses every processor, and removing it builds
without parallelism.

Output:

```text
build/bin/Debug/coduo_dedicated.exe
```

To build only the server project:

```cmd
cmake --build . --config Debug --target coduo_dedicated --parallel 4
```

To generate a solution that contains only the dedicated server, disable the
other components. The client-only dependencies (`minizip`, `SDL2`,
`libjpeg-turbo`, `libcurl`, Khronos GL headers) are then neither fetched nor
built:

```sh
cmake -A Win32 -DBUNDLED_LIBS=YES -DCODUOMP_BUILD_CLIENT=OFF -DCODUOMP_BUILD_MODULES=OFF ..
```

That solution contains `coduo_dedicated` and the bundled zlib.

The target is a console executable compiled with `LINUX_BEHAVIOR`, so it keeps
the retail Linux server's behavior. `cmake/msvc-posix/` supplies the POSIX
names the sources use (`unistd.h`, `strings.h`, `sys/time.h`, `dirent.h`) and
maps file descriptors, `stat`/`chmod`, `gettimeofday`, sleep and directory
iteration onto the CRT and Win32 APIs.

One behavior deliberately differs from the retail Linux server: localized pak
acceptance uses the Windows engine's fourteen-entry language table. The retail
Linux server accepts only `localized_english_*`, so a non-English install cannot
mount the localized pak that carries `default_mp.cfg` and startup fails with
`Couldn't load default_mp.cfg`. The standalone server now mounts
`localized_polish_*` and the other supported names for every language in that
table. `fs_ignoreLozalized 1` still disables localized assets.

### Advanced configuration

The solution can be opened in Visual Studio, or built directly with MSBuild.
The direct form is equivalent to the `cmake --build` line above and needs a
*Visual Studio Developer Command Prompt*, where `MSBuild.exe` is on `PATH`:

```sh
MSBuild.exe CoDUOMP.sln /m:4 /p:Configuration=Debug /p:Platform=Win32
```

`Release`, `MinSizeRel` and `RelWithDebInfo` are generated as well; each stages its
output in `build/bin/<config>/`:

```sh
cmake --build . --config Release --target coduo_dedicated --parallel 4
```

Configure-time options are passed at the `cmake` step as `-D<name>=<value>`:

| Option | Default | Effect |
| --- | --- | --- |
| `CODUOMP_BUILD_SERVER` | `ON` | Build `coduo_dedicated.exe`. |
| `CODUOMP_BUILD_CLIENT` | `ON` | Build `CoDUOMP.exe` in the same solution. |
| `CODUOMP_BUILD_MODULES` | `ON` | Build the cgame, UI and listen-server modules. |
| `BUNDLED_LIBS` | `ON` | Fetch and build the third-party libraries in `libs/`. |
| `CODUOMP_DEP_PREFIX` | empty | Prefix holding `include/` and `lib/` for prebuilt dependencies when `BUNDLED_LIBS=OFF`. |
| `CODUOMP_STATIC_VCRT` | `ON` | Link the MSVC runtime statically (`/MT`). |
| `CODUOMP_DISABLE_SERVER_AUTH` | `ON` | Skip external CD-key authorization for joining clients; the Makefile default is `AUTH=0`. |
| `CODUOMP_ENABLE_PUNKBUSTER` | `OFF` | Compile the PunkBuster recovery paths into the dedicated server. |

A server that authenticates joining clients and compiles PunkBuster support:

```cmd
cmake -A Win32 -DBUNDLED_LIBS=YES ^
  -DCODUOMP_DISABLE_SERVER_AUTH=OFF -DCODUOMP_ENABLE_PUNKBUSTER=ON ..
```

Extra MSBuild switches go after `--`, and `CMAKE_BUILD_PARALLEL_LEVEL` supplies
the default parallel level when `--parallel` is omitted:

```sh
cmake --build . --config Debug --parallel 4 -- /p:SomeProperty=Value
```

A single-configuration generator works from the same build directory:

```sh
cmake -G "NMake Makefiles" -DBUNDLED_LIBS=YES .. && nmake
```

The MinGW targets below remain available for the `-march=i386` and
`-march=i686` retail-baseline cross-builds.

## Windows Dedicated Server

The top-level Windows targets build a PE32 console engine and its matching PE32
server game DLL:

```sh
make windows-i386
make windows-i686
```

Outputs:

```text
build/server/windows-i386/coduo_dedicated_recovered.exe
build/game/windows-i386/uo_game_mp_x86.dll
build/server/windows-i686/coduo_dedicated_recovered.exe
build/game/windows-i686/uo_game_mp_x86.dll
```

Both targets use the 32-bit MinGW ABI and force `-O0 -mfpmath=387
-fexcess-precision=fast` in both engine and module C/C++ compilation. The
Windows engine establishes the retail-compatible 53-bit x87 precision mode at
startup and restores it after loading the game DLL. The distinction between
the targets is the generated instruction baseline: `-march=i386` or
`-march=i686`.

The engine needs MinGW zlib. If it is not installed in the compiler's default
search path, point `WINDOWS_DEP_PREFIX` at a prefix containing `include/zlib.h`
and `lib/libz.a`:

```sh
make windows-i686 WINDOWS_DEP_PREFIX=/opt/mingw-i686
```

Override the compiler names when necessary:

```sh
make windows-i386 \
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
