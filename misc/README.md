# CoDUOMP server configuration samples

Ready-to-run server configs for the dedicated engine. The ten
`example/uoconfig_example_*.cfg` files are complete single-file configs:
nine fixed game-type servers plus one general server. The remaining
`uoconfig_*.cfg` files are optional includes you can `exec` from any config.
Every cvar comment lists the accepted values.

## Files

| File | Purpose |
| --- | --- |
| `example/uoconfig_example_{tdm,dm,sd,re,bel,hq,bas,ctf,dom}.cfg` | Complete server config per game type (TDM, DM, SD, RE, BEL, HQ, BAS, CTF, DOM). |
| `example/uoconfig_example_general.cfg` | General server: game-type voting enabled, all stock maps in the rotation; a voted type persists across maps. |
| `uoconfig_gameplay.cfg` | Optional: rules, spectators, timeouts, voting, team display, all mode limits. |
| `uoconfig_weapons.cfg` | Optional: weapon/vehicle allow flags, ammo, tracers, vehicle behavior. |
| `uoconfig_physics.cfg` | Optional: movement, view heights, fall damage, bounds, bob, collision. |
| `uoconfig_maprotation.cfg` | Optional: map rotations for every game type. |

## Run

Copy the config you want into `uo/` below `fs_homepath` (default
`~/.callofduty`) or `fs_basepath`:

```sh
coduo_lnxded_recovered +exec uoconfig_example_tdm.cfg
```

```bat
coduo_dedicated_recovered.exe +exec uoconfig_example_tdm.cfg
```

To start from the startup hook instead, put `exec "uoconfig_example_tdm.cfg"`
in `uo/autoexec_mp.cfg` and start the server without arguments. The configs end
with `wait 1; map ...`; the `wait` lets the same file work from either path.

- `net_ip`, `net_port` and `net_noudp` are read before the socket opens. With
  `+exec`, pass them as `+set` values; they apply from the config only when it
  runs from `autoexec_mp.cfg`.
- `dedicated` is read-only at runtime; use `+set dedicated 1` for a LAN server
  (the default `2` advertises to the master server).
- Mods add `+set fs_game mods/<name>`.
- `g_gametype`, `sv_maxclients`, `scr_allow_jeeps`, `scr_allow_tanks` and
  `g_vehicleBurnTime` are latched (next map/restart).
- `com_hunkMegs` (hunk memory) is allocated once at startup. Pass it on the
  command line, for example `+set com_hunkMegs 128` (default 128 MiB, minimum 1).
  A value set from `+exec` only takes effect on the next start.
- `bg_fallDamage*`, `g_bounds_*`, `bg_swingSpeed` and `g_tracerchancelmg` are
  cheat protected after the game module loads; set them before the first map.
- Set `rconPassword` and `g_password` before going public.

## Logging

Samples ship with logging off (`logfile 0`, `g_log ""`).

| Setting | Values |
| --- | --- |
| `logfile` | `0` off, `1` buffered, `2` unbuffered. Writes `console_mp_server.log`, truncated at each start. |
| `g_log` | Empty disables the game log; a name such as `"games_mp.log"` appends across starts. |
| `g_logSync` | `0` buffered (tail may be lost on a kill), `1` flush every write. |

The recommended Debian setup keeps console output in journald and writes only
the game-event log to disk:

```cfg
set logfile   "0"
set g_log     "games_mp.log"
set g_logSync "1"
```

`games_mp.log` is opened in append mode, so logrotate's `copytruncate` is safe:

```text
/path/to/uo/games_mp.log {
    weekly
    rotate 4
    maxsize 50M
    missingok
    notifempty
    compress
    delaycompress
    copytruncate
}
```

Cap journald in `/etc/systemd/journald.conf.d/90-journald-global-limits.conf`
(host-wide limits for every service, not per service):

```ini
[Journal]
SystemMaxUse=500M
SystemMaxFileSize=50M
RuntimeMaxUse=200M
MaxRetentionSec=14day
SystemKeepFree=1G
```

Then restart journald and read the console with
`journalctl -u <service> -f`.

Neither engine log is rotated internally. If you enable `logfile` and get
`console_mp_server.log`, do not use `copytruncate` on it: the engine opens it in
truncate mode without append, so an external truncate can create a sparse file.
Rotate that file by restarting the service instead.

On Windows, rotate files while the server is stopped. Keep `developer`,
`sv_showCommands`, `sv_packet_info`, `showpackets` and `showdrop` off in
production; they add a line per command or packet.


## Web downloads

```text
set sv_wwwDownload "1"
set sv_wwwBaseURL "https://files.example.net/uo"
```

The engine fetches `<sv_wwwBaseURL>/<file>`, so the URL must not end with `/`
and the HTTP host must mirror the game directory. There is no fallback-URL
cvar.

## Server list and CD keys

- `dedicated 2` sends heartbeats to `coduomaster.activision.com:20610`.
  `dedicated 1` keeps the server off the list.
- The master and `coduoauthorize.activision.com:20600` hostnames are hardcoded
  in the engine. There is no `sv_master1` or similar cvar.
- Default builds use `CODUO_DISABLE_SERVER_AUTH=1`, so clients join without a
  CD key. In an `AUTH=1` build, `sv_allowAnonymous 1` asks the authorize server
  to accept keyless clients, and `net_lanauthorize 0` lets LAN clients skip
  authorization (`1` would require it for LAN too).

## Startup lines

A healthy start looks like this:

```text
execing default_mp.cfg
couldn't exec language.cfg
couldn't exec uoconfig_mp_server.cfg
couldn't exec autoexec_mp.cfg
Opening IP socket: 0.0.0.0:28960
execing uoconfig_example_tdm.cfg
couldn't exec mp_carentan_dm.cfg
Server: mp_carentan
```

`couldn't exec <file>` only means the file is absent; the server continues.
Only `default_mp.cfg` is required.

- `language.cfg` — client language override, never needed on a server.
- `uoconfig_mp_server.cfg` — the engine's own saved `seta` config, created on a
  clean exit. Not one of these samples.
- `autoexec_mp.cfg` — optional startup file; a good place for `exec` and
  machine-specific overrides.
- `mp_carentan_dm.cfg` — optional per-map config, `<map>_<mode>.cfg`.

Values and option lists were checked against the engine source and the shipped
game scripts.
