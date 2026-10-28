# Polish hints show the literal `[{+aktywacja}]` token

## Symptom

With Polish selected, use prompts display a literal command token instead of
the bound key:

```text
Naciśnij [{+aktywacja}], aby podnieść Radio polowe
```

The same text appears on vehicle, bomb and generic use hints. The Polish
controls page has no "aktywacja" action, so players cannot find or bind it.

Use itself works: `F` is bound to `+activate`.

## Root cause

The client translates a `[{command}]` marker by looking the command up in the
fixed controls-binding table and replacing the marker with the key name:

- `src/client/cgame/ui/cg_translatemessage.c` extracts the command and asks
  `UI_KeysStringForBinding`.
- `src/client/menu/ui_bindings.c` resolves it through `BindingIDFromName`,
  which only knows the 55 commands in `g_bindings`.
- `+activate` is one of them; `+aktywacja` is not.

When the lookup fails, the client deliberately keeps the raw text, so the
translated command name is printed literally. The translation changed a command
name that must remain in English.

## Affected data

Only the Polish localization is defective. The complete set is 30 replacements:

| Archive | Entry | Replacements |
| --- | --- | --- |
| `Main/localized_polish_pak1.pk3` | `localizedstrings/polish/re.str` | 2 |
| `uo/localized_polish_pakuo00.pk3` | `localizedstrings/polish/gmi_bas.str` | 2 |
| | `localizedstrings/polish/gmi_game.str` | 2 |
| | `localizedstrings/polish/gmi_vehicle.str` | 10 |
| `uo/localized_polish_pakuo01.pk3` | `localizedstrings/polish/gmi_bas.str` | 2 |
| | `localizedstrings/polish/gmi_game.str` | 2 |
| | `localizedstrings/polish/gmi_vehicle.str` | 10 |

`pakuo00` and `pakuo01` both contain the same strings and are both patched,
because search-path order decides which copy is visible.

The replacement is a pure ASCII byte substitution:

```text
[{+aktywacja}]  ->  [{+activate}]
```

Nothing else in the files changes. Single-byte Polish text is preserved.

## Other language packs

Every shipped language was audited, including the small patch archives:

- All archives pass a full CRC check.
- All `[{...}]` tokens in every language are known binding commands.
- No translated reference has a different set of command tokens than the
  English text for the same reference.
- No reference block is empty or missing its language value.

The few-KB archives (`localized_*_pak2` through `pak5`) are valid incremental
patch packs. They contain only the strings and UI assets changed by the
corresponding retail patch, so their small size is expected. They are not
truncated or corrupt.

Before the fix, every Polish archive was byte-identical to the original Polish
1.51b distribution. The three modified archives' `.bugfix-backup` copies remain
byte-identical to that reference, so only the token replacement differs.

## Fix

`tools/patch-coduo-game-data.py --fix polish` scans every PK3 below the game
root, finds `localizedstrings/polish/*.str` entries with the invalid token,
verifies the exact per-file counts above, replaces the token, rewrites the
archive, verifies its CRCs, and re-reads the result before replacing the
original. It stops on any unexpected file or count instead of forcing a global
replacement.

```sh
python3 tools/patch-coduo-game-data.py --game-root /path/to/game
python3 tools/patch-coduo-game-data.py --game-root /path/to/game --fix polish --apply --backup
```

Close the client before patching. `--backup` keeps a `.bugfix-backup` copy of
each changed archive.

## Validation

- Dry run found exactly the six archives (three per installation tree) and the
  expected counts.
- After patching, the game tree contains zero `[{+aktywacja}]` occurrences.
- A patched entry is byte-for-byte equal to the original with only the token
  substitution applied.
- A second dry run reports no remaining work.
- The language audit above reports zero unknown tokens, zero empty values and
  zero token mismatches for all eleven languages.

## Rollback

Restore each `.bugfix-backup` file over its archive, or restore the original
Polish 1.51b localization files. Localization is client-side, so a server does
not need to distribute the patched archives.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/02_polish_activate_localization.md`
