# The Polish generic pickup hint displays a literal `%s`

## Symptom

When a retrieval objective has no named objective entry, the interaction hint
that appears over the objective model reads:

```
Naciśnij [USE], aby podnieść %s cel misji
```

`%s` is shown to the player instead of being replaced by a word. The
non-generic variant, used when the objective does have a name, is correct:

```
Naciśnij [USE], aby podnieść dokumenty
```

The same Polish text is used for every generic objective trigger, so the defect
appears on every map whose retrieval objective lacks a `script_objective_name`.

## Investigation

The multiplayer retrieval script (`maps/MP/gametypes/re.gsc`, and the `uo/`
copy under `maps/mp/gametypes/re.gsc`) sets one of two hints:

```
if ( (isdefined (self.script_objective_name)) && (isdefined (level.obj[self.script_objective_name])) )
    self.trigger setHintString (&"RE_PRESS_TO_PICKUP",level.obj[self.script_objective_name]);
else
    self.trigger setHintString (&"RE_PRESS_TO_PICKUP_GENERIC");
```

`RE_PRESS_TO_PICKUP` is called with the objective name as its only message
argument, and its translation contains one `%s` for that argument:

```
REFERENCE           PRESS_TO_PICKUP
LANG_ENGLISH        "Press [{+activate}] to pick up the %s"
LANG_POLISH         "Naciśnij [{+activate}], aby podnieść %s"
```

`RE_PRESS_TO_PICKUP_GENERIC` is called with the localized string only. Its
English text has no insert, but the Polish text carries the same `%s` as the
named variant:

```
REFERENCE           PRESS_TO_PICKUP_GENERIC
LANG_ENGLISH        "Press [{+activate}] to pick up the objective"
LANG_POLISH         "Naciśnij [{+activate}], aby podnieść %s cel misji"
```

The message pipeline is `GScr_SetHintString`
(`src/server/game/script_entity_methods.c`) ->
`Scr_ConstructMessageString` (`src/server/game/script_builtins.c`) ->
`SEH_LocalizeTextMessage`
(`src/client/engine/localization/string_ed_api.cpp`).

`Scr_ConstructMessageString` walks the script parameters and writes each one
into a message buffer: a localized string is preceded by the localized-fragment
marker, a plain string by the plain-fragment marker, and user text is escaped.
`SEH_LocalizeTextMessage` then processes the buffer fragment by fragment. Each
`%s` inside a localized fragment increments `pendingInsertions`; a following
fragment is written in place of the earliest pending `%s` and decrements the
count. If the call supplies no following fragment, the `%s` is never consumed:
it stays in the output buffer exactly as written, and the hint shows it
verbatim.

So the defect is in the message data, not in the substitution code. The generic
hint has no objective name to pass, and passing an empty string would simply
produce a double space. Removing the insert from the translation is the fix
that keeps the string consistent with its caller.

Scanning every `.str` entry in `Main/` and `uo/` found exactly one
`PRESS_TO_PICKUP_GENERIC` value with an insert, the Polish one. No other
language ships a translation of this reference with a `%s`, so the fix is
Polish-only, matching the earlier invalid-key-token fix.

The defect is present in the pristine Polish 1.51b release: before this fix,
`Main/localized_polish_pak1.pk3` was byte-identical to that release
(`84eacaed335cb4c66de0f409467a18b2832f9e2a7dbc3a96acfc89bd62936209` for the
file as shipped, `039d2feda7d522a2a22aa165243ccd1ca6b6658f8fb480b1711f20c93e73b764`
after the earlier key-token fix).

## Fix

`tools/patch-coduo-game-data.py` gains the `polishformat` fix. It replaces

```
, aby podnieść %s cel misji"
```

with

```
, aby podnieść cel misji"
```

inside `localizedstrings/polish/re.str` only, and only when that byte sequence
occurs exactly once in the entry. The archive is re-read, CRC-checked, the
entry list is compared with the original, and the planner is re-run against the
rewritten archive before it replaces the original:

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix polishformat
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix polishformat --apply --backup
```

After the fix the hint reads:

```
Naciśnij [USE], aby podnieść cel misji
```

## Validation

- Dry run reported exactly one change, `localizedstrings/polish/re.str`, in
  `Main/localized_polish_pak1.pk3` in both the installation and the repaired
  `_zzzz` tree.
- Archive hash after `--apply --backup`:
  `039d2feda7d522a2a22aa165243ccd1ca6b6658f8fb480b1711f20c93e73b764` ->
  `667e6e1f0183101378e040647161e5106cb8a9c9f4387d33e8d95cf5e00d33f9`.
  (Rebuilding the fix chain from the pristine release reproduces the same
  value.)
- Re-running the fix reports `no archive needed a change`, so it is idempotent.
- The rewritten archive has the same entry count (3758) as the original; the
  zip writer emits duplicate-name warnings for pre-existing directory records
  such as `sound/` and `localizedstrings/`, but the entry list and CRCs are
  unchanged and the client loads the archive.
- Started the repaired client with `+map mp_foy`, `developer 1` and
  `logfile 1` for 45 seconds: `localized_polish_pak1.pk3 (3758 files)` is
  listed as loaded and the log contains no string or localization error.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/11_polish_generic_objective_format.md`
