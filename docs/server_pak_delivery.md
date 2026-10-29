# Shipping game fixes through server paks and the download cache

This document answers whether the existing CoD:UO server-pak and client
download machinery can deliver script and data fixes **without editing the
stock pak files that players already have**. It is based on the maintained
source in this repository and on the `server-cache` left by a real server
connection. ET:Legacy is used only as a second Q3-lineage reference for the
mod-directory convention.

## Short answer

Yes, with two different delivery classes:

1. **Server-side script fixes** can be shipped as a separate override pak on
   the server only. Name the pak with `_svr_` in its basename and the client
   never asks for it; `sv_pure 1` still accepts clients that do not have it,
   because pure verification checks the paks the *client* loads, not the paks
   the *server* loads. The server scripts come from the override pak; stock
   paks stay untouched.
2. **Client-side fixes** (menus, HUD, sound aliases, localized strings, models,
   effects) require the files on the client. They can be shipped as a new
   high-priority pak (installer), or downloaded from the server into
   `server-cache/<server>/...` and mounted by the compatibility layer. The
   stock pak files are still untouched.
3. **Never edit the stock paks** (`main/pak*`, `uo/pakuo*`, localized paks).
   They are protected by `FS_idPak`: the server refuses to stream them to
   clients, and a changed checksum makes a pure client fail.

## How the mechanism works

### 1. What the server publishes

At server startup (`SV_SpawnServer`, `server_startup.c:474-486`):

- `sv_paks` (loaded pak checksums) is published only when `sv_pure 1`.
- `sv_pakNames` (loaded pak names) likewise.
- `sv_referencedPaks` / `sv_referencedPakNames` are always published from
  `FS_ReferencedPakChecksums()` / `FS_ReferencedPakNames()`.

The referenced set is "parts of paks that this server actually used" — the
filesystem marks a pak when a file is read from it (`generalReference`,
`uiModuleReference`, `cgameModuleReference`, `gameModuleReference`). A pak
containing server `.gsc` files is referenced when the server loads those
scripts.

The client receives these keys in the systeminfo configstring and installs
them with `FS_PureServerSetLoadedPaks` / `FS_PureServerSetReferencedPaks`
(`server_message_entities.c:958-969`).

### 2. Download decision on the client

`CL_InitDownloads` (`download_protocol.c:352`):

- if `cl_allowDownload` and `sv_allowDownload` (client-side mirror) are both
  non-zero and `FS_ComparePaks(..., qtrue)` yields entries, it builds
  `clc.downloadList` in `@remote@local` form and starts `CL_NextDownload`;
- otherwise it prints the readable missing-pak list and continues to the
  map-loading path.

`FS_ComparePaks` (`filesystem_pure.c:183-275`) walks the server's referenced
pak list and skips:

- **id paks** (`FS_idPak`: `main/pak0..23`, `uo/pakuo0..23`, `mp_pak*`,
  `sp_pak*`, localized paks) — clients can never download them;
- **server paks** (`FS_serverPak`: any name containing `_svr_`) — clients do
  not even see them in the download list.

For everything else it compares the server checksum against every mounted pak
checksum. Missing/mismatched paks become `<name>.pk3` downloads; if a local
file with the same name exists, the local name becomes
`<name>.<checksum>.pk3` so the old file is preserved.

### 3. Direct download and WWW redirect

- Direct (default): `CL_BeginDownload` sends `download <remote>`; the server
  streams blocks (`SV_WriteDownloadToClient`, `server_download.c`), and the
  client writes them into the **server cache** through
  `coduomp_server_namespace_open_download_write` and renames the `.tmp` file
  when complete (`server_message_entities.c:893-926`).
- WWW redirect (`sv_wwwDownload 1`, `sv_wwwBaseURL <url>`): the server sends a
  redirect block instead; the client uses libwww (`download.c`,
  `download_protocol.c`) and stores the finished file through
  `coduomp_server_namespace_build_download_path`. `sv_wwwDlDisconnected`
  controls whether the client disconnects during the download.

`cl_wwwDownload` (default 1) is advertised in the userinfo so the server knows
the client can handle a redirect (`server_client_message.c:330-334`).

### 4. Where downloaded files live and how they are mounted

The dirty tree contains evidence from a real connection:

```
coduo_bin_modified\server-cache\codhost\teufel_mods\fun_bots\z_menus.pk3
```

This is a downloaded mod pak: the server key, the server's mod path
(`teufel_mods/fun_bots`) and the pak name. The cache is persistent and
preserves the virtual path.

Bug 21 changed the server key: a live connection now uses
`<address>-<port>-<hostname>` (for example
`server-cache/127-0-0-1-28961-codhost/...`) instead of the advertised hostname
alone, because the default `CoDHost` name let unrelated servers share one
cache. Existing display-name caches are still tried by the cached-demo path,
then a suffix match is used for the new endpoint-keyed directories.

`server_namespace_provider.c` builds `server-cache/<sanitized server name>` as
the active home root; `coduomp_server_namespace_add_game_directory` mounts it
with `FS_AddLocalizedGameDirectory`. `FS_AddSearchPath`
(`filesystem_catalog_core.c:106-121`) **prepends** non-localized search paths,
so the cache is mounted *above* the stock `main`/`uo` paks. That is what makes
a downloaded pak able to override a stock file with the same virtual path.

## Pure-server semantics (the important part)

- `sv_paks` is the server's **loaded** pak checksum list; it is sent only when
  `sv_pure 1`.
- `CL_SendPureChecksums` sends the client's `FS_ReferencedPakPureChecksums()`
  (the paks the client referenced) plus the two stock checksums.
- `SV_VerifyPaks_f` (`server_client_message.c:163-275`) checks that every
  checksum the **client** sent is present in the server's loaded list. It does
  **not** require the client to have every pak the server loaded.
- On the client, `FS_PakIsPure` (`filesystem_pure.c:34-47`) makes a pak's files
  inaccessible if its checksum is not in the server's `sv_paks` list, and loose
  files are ignored while connected to a pure server (except
  `FS_PureIgnoresExtension` entries).

Consequences:

| Pak location | Client downloads it? | Pure `1` client can join? | Client can use its files? |
| --- | --- | --- | --- |
| Server-only `*_svr_*.pk3` | No (`FS_serverPak` skip) | Yes (client may omit server paks) | No (not present) |
| Server+client `zzz_fix.pk3` (referenced) | Yes, if `cl_allowDownload 1` | Yes after download; also yes if the client refuses download, but then the client cannot use its files | Only after download |
| Stock `main/pak*` / `uo/pakuo*`, edited | Never (`FS_idPak` block) | No — checksum mismatch | — |
| Stock pak plus separate override pak in installer | No | Yes | Yes (override mounts above stock) |

This is the key result: **a server-only script fix can be shipped without any
client download even with `sv_pure 1`**, because pure only rejects *extra*
client paks and inaccessible files, not missing server paks. The `_svr_`
basename is what keeps it out of the client's download list.

## What each kind of fix needs

| Fix kind | Examples from this project | Where it must run | Delivery |
| --- | --- | --- | --- |
| Server gametype/script logic | bug 01 RETRIEVAL, silent sound calls, scoring, spawn logic | Server | `*_svr_*.pk3` override (no client download) or normal referenced pak |
| Client sound aliases | bug 16 demotion alias (CSV) | Client | client pak (installer or download) |
| Client UI/menus/localization | bugs 02, 09, 10, 11, 12, 14 | Client | client pak (installer or download) |
| Client HUD/effects/models | bug 17 effect, bug 18 sun/sky, missing textures | Client | client pak (installer or download) |
| Both server and client | a gametype script plus a new alias/asset | Both | one referenced pak downloaded by clients, or two paks (server `_svr_` + client) |

A combined pak that contains both server and client fixes will be referenced
and therefore downloaded by clients (unless `_svr_`, which then hides it from
clients and prevents client-side fixes from reaching them). Splitting the two
classes is the cleanest shipping model.

## Recommended packaging

### Option A — override pak (least invasive, best for this project)

1. Create `zzz_coduomp_svr.pk3` containing only server scripts
   (`maps/mp/gametypes/*.gsc`, server-side map scripts). The `_svr_` token in
   the basename makes clients skip it. Put it in the server's `uo/` (or mod
   directory). `sv_pure 1` is fine; no client download, no client changes.
2. Create `zzz_coduomp_client.pk3` containing client assets
   (`ui_mp/...`, `soundaliases/...`, `localizedstrings/...`, shaders/effects,
   models). Ship it in the client installer (preferred) or let clients download
   it from the server (referenced pak, non-`_svr_`). If downloaded, it lands in
   `server-cache/<server>/` and mounts above the stock paks.
3. Leave `main/pak*.pk3` and `uo/pakuo*.pk3` untouched. This preserves the
   checksums that pure clients expect, avoids the id-pak download block, and
   keeps the original files as the fallback.

### Option B — a dedicated mod directory (ET:Legacy-style)

ET:Legacy ships `etmain` (base) unchanged and distributes its changes as a
separate `fs_game` mod (the "legacy" entry in the browser). CoD:UO supports the
same idea: put the fixes in a `coduomp/` (or `coduomp_fixes/`) mod directory and
set `fs_game` on the server. The server browser then lists a distinct mod, the
mod's paks are referenced/downloaded through the same flow, and the stock
`main`/`uo` trees remain untouched.

Trade-offs: a mod requires server operators to select it and clients to switch
to it; a plain override pak works with existing servers/profiles. For a
maintained fix line, Option A is the smaller step; Option B is the better
long-term identity if the project wants a visible "coduomp" mod like ET:Legacy.

## What the dirty cache proves

`server-cache\codhost\teufel_mods\fun_bots\z_menus.pk3` shows:

- the client did download a third-party pak automatically;
- the cache directory is derived from the server name and keeps the server's
  mod path;
- the download is persistent and can be remounted later (the provider also
  supports cached-demo activation);
- the mechanism is already used by real servers, so the plumbing is exercised.

## Risks and limits

- A downloaded pak can override stock files only while the cache is active for
  that server and only if its checksum is in the server's `sv_paks` list
  (`FS_PakIsPure`). If a client joins a different server, the old cache is not
  mounted (per-server isolation).
- `cl_allowDownload 0` clients will not receive a referenced client fix pak;
  they can still play if they already have the files or if the fix is
  server-only.
- A client that has an extra pak (e.g., its own override) will fail pure if
  that pak's checksum is not in the server's loaded list. Shipping a client pak
  therefore requires the server to load the same pak, or the client to play on
  `sv_pure 0` servers.
- Editing a stock pak changes its checksum. The server will refuse to stream it
  (`FS_idPak`) and pure clients will be rejected. This is why fixes must be
  separate override paks.
- The redirect path is HTTP-only in this engine (`libwww`); there is no
  `sv_dlURL` cvar equivalent in the recovered code — the CoD names are
  `sv_wwwDownload` / `sv_wwwBaseURL`.

## End-to-end validation performed (local two-process test)

A dedicated server and a separate client process were used (a listen server
skips `CL_InitDownloads`). Both used isolated `fs_homepath` directories; the
server loaded two purpose-built test paks:

1. **Server-only pak**: `zzz_testsvr_svr_1.pk3` containing a `tdm.gsc` copy
   with a marker cvar. With `sv_pure 1`, the server used it
   (`scr_marker_test 1`), the client joined without downloading it, and the
   `_svr_` pak was absent from the client cache.
2. **Client fix pak**: `zzz_testclient.pk3` containing a `mp_foy.gsc` copy with
   a marker cvar (a referenced non-`_svr_` pak). The server used it
   (`scr_client_pak_test 1`); the client reported
   `Need paks: @uo/zzz_testclient.pk3@...`, downloaded it, and the file appeared
   at `server-cache/127-0-0-1-28961-codhost/uo/zzz_testclient.pk3` (2103
   bytes). The client reached `cl_ingame 1`.

This confirms both delivery classes and the endpoint-keyed cache (bug 21).
Remaining before shipping:

- repeat the two tests against a server on another host and, if used, an HTTP
  redirect (`sv_wwwDownload 1` + `sv_wwwBaseURL`);
- decide whether the client fix pak ships in the installer or is always
  downloaded (the former avoids a first-connect wait).

## File references

- `src/client/engine/client/download_protocol.c` — `CL_InitDownloads`,
  `CL_NextDownload`, `CL_WWWDownload`, `CL_DownloadsComplete`
- `src/client/engine/client/server_message_entities.c` — pure systeminfo,
  direct download write/rename
- `src/filesystem/filesystem_pure.c` — `FS_idPak`, `FS_serverPak`,
  `FS_ComparePaks`, `FS_PakIsPure`, pure checksum lists
- `src/server/engine/server_client_message.c` — `SV_VerifyPaks_f`
- `src/server/engine/server_download.c` — download/WWW redirect responses
- `src/server/engine/server_startup.c` — `sv_paks` / `sv_referencedPaks`
- `src/client/engine/filesystem/server_namespace*.c` — `server-cache` root,
  download paths, mount order
- `src/filesystem/filesystem_catalog_core.c` — `FS_AddSearchPath` prepend order
- `coduo_bin_modified/server-cache/codhost/teufel_mods/fun_bots/z_menus.pk3` —
  real downloaded pak
- `C:\dev\projects\github\etlegacy` — `etmain` base + separate `fs_game` mod
  model and Q3-lineage pure/download flow
