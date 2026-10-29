# Stale global server addresses and the master query latch

## A moved server leaves its previous address

### Symptom

A server's public address changes (for example, the router receives a new WAN
address). The server re-registers with the master and appears under the new
address, but the client keeps the previous address in its global server list for
the rest of the session. The old row never answers, so it is not drawn, but it
still occupies a record, counts toward `LAN_GetServerCount`, takes part in the
address sort, and is written to or rejected from `uoservercache.dat` only when
the UI module shuts down.

### Investigation

The discoverability path itself works:

- A dedicated server sends `heartbeat COD-1` and `getstatus` to
  `coduomaster.activision.com:20610`. The master records the packet source
  address, so the next heartbeat after a WAN change registers the new address
  without any server-side configuration. The heartbeat interval is 180 s
  (`SERVER_MASTER_HEARTBEAT_INTERVAL_MSEC`).
- The client resolves the same hostname on every full refresh and sends
  `getservers <protocol> full empty`.
- The browser only lists a server after it answers `getinfo`; a record whose
  address no longer exists is skipped by `UI_BuildServerDisplayList`, which is
  why the stale row is invisible.

The remaining defect is retention. This client keeps the global records between
refreshes (the cached list is loaded at UI start) and stamps each record with
`masterResponseAge`. `CL_FindServerInfo` resets a record, including its age,
whenever the master lists that address again; `CL_GlobalServers_f` increments
every age before each query. Nothing consumed the age inside a session except
`LAN_SaveServersToCache`, and that function runs from `UI_Shutdown`. An address
that the server no longer uses therefore survived every refresh until the UI
module was torn down.

The upstream Call of Duty 1 source clears the global list on each refresh
(`cls_numglobalservers = -1` in `CL_GlobalServers_f`, reset to zero by the first
response packet), so it cannot retain a moved address. This client deliberately
keeps the list and ages it instead, but the aging result was applied too late.

A master response itself is not a safe place to drop records. The master splits
one list across packets and a captured response can contain `\EOT` in one
packet and further addresses in a following packet (`\EOF`), so the end marker
does not prove that the list is complete.

### Fix

- The stale/duplicate scan that was inside `LAN_SaveServersToCache` is now
  `LAN_RemoveStaleServers`. It keeps the original rules: a record is removed
  when `masterResponseAge` reaches
  `LAN_SERVER_RECENT_MASTER_RESPONSE_AGE_LIMIT` (three consecutive responses
  without that address), and adjacent duplicate addresses are removed. A
  removed slot takes the current last record and the active prefix is restored
  to address order.
- `CL_GlobalServers_f` calls `LAN_RemoveStaleServers` before it ages the
  survivors. By then every packet of the previous response set has been
  processed, and the three-response tolerance still absorbs a lost master
  response.
- `LAN_SaveServersToCache` calls the same helper, so the written cache keeps its
  previous filtering behavior.

### Validation

- Built the 32-bit client; no layout or ABI change is involved.
- A record is reset by `CL_FindServerInfo` on every response that contains its
  address, so a server that is still listed never accumulates age and is never
  removed.
- The age counter saturates at `UINT8_MAX`; the comparison is
  `>= 3`, so saturated records are removed rather than wrapping.
- The removal loop runs from the end of the list, so a slot swapped in from the
  tail has already been examined and cannot be skipped.
- Manual check: with the server running, note its address in the browser, change
  the address, refresh the browser until the new address is listed. The old
  address must disappear after at most three refreshes without it, and the
  reported server count must drop with it.

### Notes

The change does not make an unreachable server visible, and it does not make the
browser wait for the master. It only stops the client from carrying an address
that the master has stopped reporting.

### Files

- `src/client/engine/client/server_browser.c`
- `docs/bugfixes/04_server_address_and_master_query.md`

## A failed master query leaves the browser waiting

### Symptom

Choosing the internet server list can leave the browser on its "waiting for
master" state for the rest of the session. The list never fills in, including
servers that are still in the local cache, and a later refresh does not recover.

### Investigation

`CL_GlobalServers_f` arms `cls.waitingForMasterResponse` and then builds and
sends the `getservers` query. The latch is only ever cleared by
`CL_ServersResponsePacket`, that is, by a packet from the master.

Three ways to reach a state with the latch armed and no query in flight:

- `NET_StringToAdr` can fail. It reports failure by returning `qfalse` and
  setting the address type to `NA_BOT`; the return value was ignored. The caller
  then overwrote the port and handed the address to `NET_OutOfBandPrint`, and
  `NET_SendPacket` silently drops `NA_BOT` without sending anything. Nothing
  produces the response that would clear the latch.
- The query or the `fs_restrict` suffix can overflow the 1 KiB query buffer.
  Those paths returned after the latch had already been armed.
- Even a successfully sent query has no bound. If the master is unreachable or
  a UDP reply is lost, no response arrives and the latch stays set.

The latch is not cheap. `LAN_WaitServerResponse` returns it for the global
source, and `UI_BuildServerDisplayList` returns immediately while it is true, so
the browser does not rebuild the list at all. `UI_DoServerRefresh` also stops
honoring the wait after its own five-second window, but that only lets ping
updates run; the display list still returns early, so cached servers remain
invisible.

The same ordering made an unrelated failure destructive: every call aged the
stored records before the query was known to be sendable. Repeated query
failures (bad `debug_protocol` length, for example) would age records three
times and let `LAN_RemoveStaleServers` drop addresses that the master had not
actually stopped reporting.

### Fix

- Build the complete query first, including every appended keyword and the
  `fs_restrict` suffix. An oversize query now returns before any state changes
  and clears the latch so an earlier stuck wait cannot survive.
- Resolve the master before arming anything. When `NET_StringToAdr` fails or
  returns a type other than `NA_IP`, print
  `Couldn't resolve address: coduomaster.activision.com`, clear the latch and
  return.
- Age and prune the stored records, and reset `numGlobalServerAddresses` and
  the ping source, only after a sendable query exists.
- `clientStatic_t` gained `masterResponseRequestTime`, set when the query is
  sent. `LAN_WaitServerResponse` treats the wait as over after five seconds,
  the same interval the UI already uses for its refresh window, and clears the
  latch. A response that arrives later is still parsed and merged normally.

The new field is appended to the end of `clientStatic_t`, so every original
layout offset asserted in `cgame.h` is unchanged; only the clear-extent
assert grows from `0x42d0ec` to `0x42d0f0`. `CL_Shutdown` clears the struct with
`memset`, so the field needs no separate reset.

### Validation

- Built the 32-bit client and ran the source-policy check.
- Layout asserts cover the appended field; the tail extent assert fails at
  compile time if the field is moved.
- A DNS failure for `coduomaster.activision.com` now prints the resolve error
  and lets the display list build from the remaining records instead of
  blocking.
- An already-armed wait expires on the first `LAN_WaitServerResponse` call
  after five seconds, and `masterResponseRequestTime` is refreshed on the next
  query.
- Successful refreshes are unchanged: the first response packet clears the
  latch before the timeout matters.

### Notes

The timeout does not treat an unanswered query as an empty master list. It only
releases the UI wait; stored records keep their age and are removed by the
normal three-response rule once later queries actually reach the master.

### Files

- `src/client/engine/client/server_browser.c`
- `src/client/engine/client/cgame.h`
- `docs/bugfixes/04_server_address_and_master_query.md`
