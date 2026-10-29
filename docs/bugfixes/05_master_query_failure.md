# A failed master query leaves the server browser waiting forever

## Symptom

Choosing the internet server list can leave the browser on its "waiting for
master" state for the rest of the session. The list never fills in, including
servers that are still in the local cache, and a later refresh does not recover.

## Investigation

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

## Fix

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

## Validation

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

## Notes

The timeout does not treat an unanswered query as an empty master list. It only
releases the UI wait; stored records keep their age and are removed by the
normal three-response rule once later queries actually reach the master.

## Files

- `src/client/engine/client/server_browser.c`
- `src/client/engine/client/cgame.h`
- `docs/bugfixes/05_master_query_failure.md`
