# Server browser truncates colored hostnames by raw byte count

## Symptom

A server advertised a colored `sv_hostname`, for example:

```text
^1Red ^2Green ^3Blue Long Server Name
```

The browser showed a shortened name and, with enough color codes, could end in
a stray `^` or show the wrong color for the remaining text. Plain names were
unaffected. Favorites and the find-player list inherited the same shortened
name.

## Root cause

The client stored every received hostname in a fixed 32-byte field:

```c
LAN_SERVER_HOSTNAME_SIZE = 32,
...
char hostName[LAN_SERVER_HOSTNAME_SIZE]; /* +0x2e */
```

`CL_SetServerInfo` and `LAN_AddServer` copied it with a raw byte bound:

```c
strncpy(server->hostName, ..., sizeof(server->hostName) - 1);
server->hostName[sizeof(server->hostName) - 1] = '\0';
```

Every `^n` color pair is part of the string, so each pair removed two bytes
from the space available for visible characters. With eight color pairs the
browser retained only 15 visible bytes out of 31. Because the cut was made on a
raw byte count, it could also split a `^n` pair and leave malformed color
state.

The UI cannot recover characters that were already discarded:
`LAN_GetServerInfo` republishes only the stored, truncated value.

## Fix

- `LAN_SERVER_HOSTNAME_SIZE` is now 96 bytes, so the record can hold a long
  colored name. `lan_server_info_t` grows from `0x98` to `0xd8` bytes; every
  field offset and the `clientStatic_t` server-array offsets were updated
  accordingly.
- A new bounded copy, `LAN_CopyHostName`, copies at most
  `destinationSize - 1` bytes, always NUL-terminates, never reads past the
  source terminator, and never splits a `^` from the byte that follows it. If
  the next color pair does not fit, the copy stops cleanly before it.
- Both hostname paths use the helper: network responses in `CL_SetServerInfo`
  and local/global/favorite entries in `LAN_AddServer`.
- The server cache version is now 2. A cache written with 32-byte records is
  rejected by the version or serialized-size check and rebuilt, instead of
  being interpreted with the wrong record size.

Find-player still copies a `pendingServerStatus_t.name[64]` value into a
64-byte display slot. That path therefore keeps a 63-byte cap, but the main
server list, favorites and sorting now see the full stored name. The 64-byte UI
storage is a retail syscall layout and is intentionally left unchanged.

## Validation

- Built the 32-bit MSVC client with the new static assertions.
- Static layout: `hostName` at `0x2e` with extent `0x60`, `mapName` at `0x8e`,
  `game` at `0xae`, `gameType` at `0xc6`, record size `0xd8`.
- `clientStatic_t` offsets were recomputed for the 128 local, 20000 global and
  128 favorite records and are pinned by static assertions.
- The helper's loop condition guarantees `output + 1 <= destinationSize` at
  termination; a color pair is copied only when both bytes fit.

## Files

- `src/client/engine/client/server_browser.h`
- `src/client/engine/client/server_browser.c`
- `src/client/engine/client/cgame.h`
