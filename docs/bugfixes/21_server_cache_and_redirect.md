# Server download cache identity and redirect URL joining

## Server caches collide on the advertised hostname

### Symptom and impact

The compatibility layer stores downloaded server files under
`server-cache/<server>/...` and mounts that directory **above** the stock
`main`/`uo` paks. The directory name was derived only from the server's
advertised hostname (`sv_hostname` / `hostname`, sanitized).

Two unrelated servers that use the same name — and `CoDHost` is the default
name of a freshly started server — share one cache. Consequences:

- a mod pak or config downloaded from server A is still present when the
  player later joins server B;
- a malicious server can pre-seed the cache directory of a trusted name, and
  the cached files are mounted with higher priority than the stock paks;
- under `sv_pure 0` a cached pak can shadow stock menus, scripts or assets;
  under `sv_pure 1` the checksum filter blocks cached *paks* that the server
  did not publish, but the cached loose config files and any pure-0 session
  remain exposed;
- cache cleanup and demo playback can select the wrong server's files.

A local cache contained direct evidence of the collision:

```
server-cache\codhost\teufel_mods\fun_bots\z_menus.pk3
```

`codhost` is the default hostname, not a unique endpoint.

### Root cause

`CL_ParseGamestate` (`src/client/engine/client/server_message_entities.c`)
chooses the namespace from the display name:

```
const char *serverDisplayName = cls.serverName;
advertisedName = Info_ValueForKey(serverInfo, "sv_hostname");
...
coduomp_server_namespace_activate(serverDisplayName, ...);
```

`coduomp_namespace_sanitize_name` in
`src/client/engine/filesystem/server_namespace_provider.c` then turns only that
string into the cache directory. Nothing binds the directory to the server's
address or port, even though `clc.serverAddress` is available at this point.

### Fix

The namespace key is now the endpoint plus the display name:

```
char namespaceName[MAX_QPATH];
Com_sprintf(namespaceName, sizeof(namespaceName), "%s-%s",
            NET_AdrToString(clc.serverAddress), serverDisplayName);
coduomp_server_namespace_activate(namespaceName, ...);
```

`NET_AdrToString` includes the port, and the address is placed first so a long
hostname cannot truncate the unique part during sanitization. A local test
server now produces:

```
server-cache/127-0-0-1-28961-codhost/uo/zzz_testclient.pk3
```

The `NOT_FROM_ORIGINAL_SOURCE` comment records why the compatibility layer
keys the cache this way.

#### Cached-demo follow-up

`coduomp_server_namespace_activate_cached` previously resolved a demo by the
bare display name. Live caches now carry the endpoint prefix, so the function
first tries the exact name (old caches still work) and then a suffix match
against `-<sanitized name>` for the new directories. If several servers with
the same name have caches, the lexicographically first match is used; the demo
metadata does not carry the endpoint address, so this remains a best effort.

### Validation

A local dedicated server and a separate client process were used (a listen
server skips `CL_InitDownloads`). Both used isolated `fs_homepath` directories;
the server loaded two purpose-built test paks:

- `zzz_testsvr_svr_1.pk3` — a `maps/mp/gametypes/tdm.gsc` copy with
  `setCvar("scr_marker_test", "1")`; the `_svr_` name keeps it out of the
  client download list;
- `zzz_testclient.pk3` — a `maps/mp/mp_foy.gsc` copy with
  `setCvar("scr_client_pak_test", "1")`; a referenced non-`_svr_` pak.

Results with `sv_pure 1`:

- server log: `cvar set scr_marker_test 1` and
  `cvar set scr_client_pak_test 1` — the server used both overrides;
- client log: `Connected to a pure server`,
  `Need paks: @uo/zzz_testclient.pk3@uo/zzz_testclient.pk3`,
  `CL_BeginDownload`, then the file appeared at
  `server-cache/127-0-0-1-28961-codhost/uo/zzz_testclient.pk3` (2103 bytes);
- the `_svr_` pak was **not** in the client cache and was not requested;
- the client reached `cl_ingame 1`.

The same test before the fix used `server-cache/codhost/...`, confirming the
old collision and the new endpoint key. A normal `mp_foy` client start with the
rebuilt binary has no new errors. `make policy-check` passes.

### Files

- `src/client/engine/client/server_message_entities.c`
- `src/client/engine/filesystem/server_namespace_provider.c`
- `docs/server_pak_delivery.md`
- `docs/bugfixes/21_server_cache_and_redirect.md`

## Redirect downloads insert a doubled separator

### Symptom

A server configured with an HTTP redirect base URL that ends in `/` (the
natural way to write a directory URL) builds a download URL with two
separators:

```
http://host/base//uo/pak.pk3
```

Some HTTP servers and proxies reject the doubled path, so the client's
redirected download fails even though the same file downloads from the base
URL without the trailing slash. Backslashes in the virtual pak path can
produce an invalid URL for the same reason.

### Root cause

`SV_WriteDownloadToClient` concatenated the strings directly:

```
va("%s/%s", sv_wwwBaseURL->string, client->download.fileName)
```

There is no normalization of the configured base or of the relative pak path.
The redirect URL is sent to the client verbatim, so the client requests the
doubled path.

### Fix

`server_download.c` now builds the URL through
`coduomp_www_build_redirect_url`:

- strips every trailing `/` or `\` from the configured base;
- strips leading separators from the pak path;
- converts `\` to `/` in the pak path;
- joins them with exactly one `/`;
- leaves the URL empty when the base or path is empty.

An empty result now logs a diagnostic and falls through to the normal direct
download instead of stalling the client. The regular non-empty path sends the
same redirect block as before.

### Configuration note

The URL must be set from a config file, not on the command line, because the
engine treats `//` in a command-line token as the start of a comment:

```
set sv_wwwBaseURL "http://127.0.0.1:8082/"
```

```
+exec www.cfg
```

Putting `+set sv_wwwBaseURL http://host/base/` directly on the command line
truncates the remaining `+map` command. This is a separate usability issue
(queued) but it affects how the redirect is tested.

### Validation

- The MSVC client and the MinGW dedicated server both compile with the change.
- `make -f build-mk/server.mk windows-i686` builds the shipped server and the
  PE stack check passes; `make policy-check` passes.
- End-to-end redirect validated with a dedicated server and a clean
  client:

  ```
  # server config (www.cfg), executed with +exec
  set sv_wwwBaseURL "http://127.0.0.1:8085/"

  # server log
  Redirecting client 'WWWTest' to http://127.0.0.1:8085/uo/zzz_testclient.pk3

  # client log
  Server redirected download: http://127.0.0.1:8085/uo/zzz_testclient.pk3
  cvar set cl_downloadCount 2103

  # HTTP server
  "GET /uo/zzz_testclient.pk3 HTTP/1.1" 200 -

  # client cache
  server-cache/127-0-0-1-28993-codhost/uo/zzz_testclient.pk3   2103 bytes
  ```

  The base URL ends in `/` and the virtual pak path starts with `uo/`; the
  resulting request still contains exactly one separator between them, so the
  file downloads into the endpoint-keyed cache and the pure check passes.
- The same run also confirms the empty-base branch is not taken while a base is
  configured: the redirect block is sent and the direct-download path is not.
- An earlier attempt at this test was blocked by the dedicated-server stack
  overflow that the Windows server link flags now prevent; the crash happened
  during map start, before any client could connect.

### Files

- `src/server/engine/server_download.c`
- `docs/bugfixes/21_server_cache_and_redirect.md`
