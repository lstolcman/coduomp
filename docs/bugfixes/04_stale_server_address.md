# A moved server leaves its previous address in the browser list

## Symptom

A server's public address changes (for example, the router receives a new WAN
address). The server re-registers with the master and appears under the new
address, but the client keeps the previous address in its global server list for
the rest of the session. The old row never answers, so it is not drawn, but it
still occupies a record, counts toward `LAN_GetServerCount`, takes part in the
address sort, and is written to or rejected from `uoservercache.dat` only when
the UI module shuts down.

## Investigation

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

## Fix

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

## Validation

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

## Notes

The change does not make an unreachable server visible, and it does not make the
browser wait for the master. It only stops the client from carrying an address
that the master has stopped reporting.

## Files

- `src/client/engine/client/server_browser.c`
- `docs/bugfixes/04_stale_server_address.md`
