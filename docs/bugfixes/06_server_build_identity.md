# The dedicated server reports a stale build identity

## Symptom

The dedicated server's first log line always claimed the same build:

```
CoD:UO Recovered Dedicated Engine 1.51 (windows-i386) build Jul 01 2026
```

The date was a literal in `com_init_runtime.c`, so it did not describe the
binary that was actually running, and there was no way to tell which source
revision produced a build when reading a server log or a user's report.

## Investigation

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

## Fix

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

## Validation

- Built `server-engine64` under WSL and ran it briefly; the banner reported
  `build Oct 28 2026 02:19:26 git 3e1c6e9a-dirty`.
- Built `server-windows-i686` with the same define; the resulting
  `coduo_dedicated_recovered.exe` contains the revision and compile date
  strings and no longer contains `Jul 01 2026`.
- The client targets are untouched; their banner stays
  `Open CoD:UO 1.51 build <date>`.
- `make policy-check` passes.

## Notes

`__DATE__` and `__TIME__` describe when the banner translation unit was last
compiled. An incremental build that does not recompile `com_init_runtime.c`
keeps the earlier timestamp, which is the same behavior as the client banner.
`-dirty` is intentional: a server built from a modified tree must not claim to
be the recorded commit.

## Files

- `src/server/standalone/core_runtime/coduomp_build_info.h`
- `src/server/standalone/core_runtime/com_init_runtime.c`
- `build-mk/server.mk`
- `docs/bugfixes/06_server_build_identity.md`
