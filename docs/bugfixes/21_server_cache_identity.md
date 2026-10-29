# Server download caches collide on the advertised hostname

## Symptom and impact

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

The dirty working tree contains direct evidence of the collision:

```
server-cache\codhost\teufel_mods\fun_bots\z_menus.pk3
```

`codhost` is the default hostname, not a unique endpoint.

## Root cause

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

## Fix

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

### Cached-demo follow-up

`coduomp_server_namespace_activate_cached` previously resolved a demo by the
bare display name. Live caches now carry the endpoint prefix, so the function
first tries the exact name (old caches still work) and then a suffix match
against `-<sanitized name>` for the new directories. If several servers with
the same name have caches, the lexicographically first match is used; the demo
metadata does not carry the endpoint address, so this remains a best effort.

## Validation

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

## Files

- `src/client/engine/client/server_message_entities.c`
- `src/client/engine/filesystem/server_namespace_provider.c`
- `docs/server_pak_delivery.md`
- `docs/bugfixes/21_server_cache_identity.md`
