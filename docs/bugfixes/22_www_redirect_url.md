# Redirect downloads insert a doubled separator

## Symptom

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

## Root cause

`SV_WriteDownloadToClient` concatenated the strings directly:

```
va("%s/%s", sv_wwwBaseURL->string, client->download.fileName)
```

There is no normalization of the configured base or of the relative pak path.
The redirect URL is sent to the client verbatim, so the client requests the
doubled path.

## Fix

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

## Configuration note

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

## Validation

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

## Files

- `src/server/engine/server_download.c`
- `docs/bugfixes/22_www_redirect_url.md`
