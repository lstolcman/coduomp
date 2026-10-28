# Localized hints and messages show doubled quotation marks

## Symptom

With the Polish language selected, the weapon-use hints are drawn with a pair
of quotation marks that belong to the data file rather than to the sentence:

```
"Naciśnij [%s], aby użyć DP28"
"Naciśnij [%s], aby użyć MG34"
```

Other Polish messages have a stray quote on one side only:

```
"%s upuścił flagę do wody.
"Naciśnij [%s], aby zabrać ważne dokumenty.
Towarzysze! ... z radzieckiej ziemi!"
```

The French Hurtgen objective description ends the same way:

```
Le carré bleu représente le camp allié."
```

The quote is not part of the translated sentence. It is a second copy of the
delimiter that is still stored inside the string.

## Investigation

A `.str` file stores each translation as one `LANG_*` line where the value is a
quoted literal:

```
REFERENCE           USEDP28
LANG_ENGLISH        "Press [%s] to use the DP28"
LANG_POLISH         ""Naciśnij [%s], aby użyć DP28""
```

The loader is `CStringEdPackage::InsideQuotes`
(`src/client/engine/localization/string_ed_package.cpp`, recovered from
`CoDUOMP.exe` `0x00471b60..0x00471cff`). It does exactly one pass:

1. skip leading spaces and tabs;
2. if the next character is `"`, skip that one character;
3. copy the rest;
4. trim trailing spaces and tabs;
5. if the last character is `"`, drop that one character.

It does not loop, so a doubled delimiter only loses one layer. `""text""`
becomes `"text"`, which is what the player sees. The same routine feeds
`ParseLine` for both `REFERENCE` and `LANG_*` values, and the resulting string
is what the cgame and game DLL receive from `SE_StringEd_GetString`.

Not every doubled quote is a defect. A translation may legitimately open or
close with a quotation mark that is part of the sentence, for example

```
LANG_ENGLISH        ""We happy few, we band of warriors/..." - William Shakespeare"
LANG_ENGLISH        ""RatTums" the Hamster"
```

Here English itself starts with `""` (delimiter plus the opening quotation
mark), so the Polish value with the same shape is correct. The reliable test is
to compare the leading and trailing quote shape of each translation with the
`LANG_ENGLISH` value of the same reference: an extra leading or trailing `"`
that English does not have is a translation mistake.

Scanning every `.str` entry in `Main/` and `uo/` with that rule produced nine
extra delimiters, all of them in Polish or French:

| Archive | File | Reference | Extra |
| --- | --- | --- | --- |
| `localized_polish_pakuo00.pk3` | `localizedstrings/polish/gmi_cgame.str` | `USE50CAL` | leading and trailing |
| `localized_polish_pakuo00.pk3` | `localizedstrings/polish/gmi_cgame.str` | `USEDP28` | leading and trailing |
| `localized_polish_pakuo00.pk3` | `localizedstrings/polish/gmi_cgame.str` | `USESG43` | leading and trailing |
| `localized_polish_pakuo00.pk3` | `localizedstrings/polish/gmi_cgame.str` | `USE30CAL` | leading and trailing |
| `localized_polish_pakuo00.pk3` | `localizedstrings/polish/gmi_cgame.str` | `USEMG34` | leading and trailing |
| `localized_polish_pakuo00.pk3` | `localizedstrings/polish/gmi_ctf.str` | `FLAG_INWATER` | leading |
| `localized_polish_pakuo00.pk3` | `localizedstrings/polish/gmi_script.str` | `HINTSTR_GET_DOCUMENTS` | leading |
| `localized_polish_pakuo00.pk3` | `localizedstrings/polish/pi_ponyri.str` | `COMMISSAR_ENDING` | trailing |
| `localized_french_pak2.pk3` | `localizedstrings/french/re_obj.str` | `HURTGEN_SPAWN_ATTAKER` | trailing |

The same Polish strings are duplicated in `localized_polish_pakuo01.pk3`
(`gmi_bas.str`, `gmi_game.str`, `gmi_vehicle.str`) for the second language
layer; the scanner covered both archives and only the entries above matched.

Before the fix, these three archives were byte-for-byte identical to the
pristine Polish 1.51b release, so the defect is in the shipped translation and
not in this installation:

```
localized_french_pak2.pk3      5560186a5e7f9847c1468b3c4f21c55df9089a09d220223657139cdb15e86964
localized_polish_pakuo00.pk3   30a98fadc80ac4ab3476e4e7443c0eb7ef94ff1c9cd8595894a24e3daf1e8e73
localized_polish_pakuo01.pk3   814a1f58899e2b57881920f11ea47cfa3787cd0de9f5dcb344cb4aba16d6f7e3
```

## Fix

`tools/patch-coduo-game-data.py` gains the `polishquotes` fix. It carries a
table of exact byte pairs (file suffix, stored literal, corrected literal) and
rewrites an entry only when the stored literal occurs exactly once in the
entry. A missing or duplicated match aborts the archive instead of guessing.
The archive is re-read, CRC-checked, and the planner is re-run against the
rewritten archive before it replaces the original, so a partial patch cannot be
left behind.

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix polishquotes
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix polishquotes --apply --backup
```

Displayed text with the loader model above, before and after the fix:

| Reference | Before | After |
| --- | --- | --- |
| `USE50CAL` | `"Naciśnij [%s], aby użyć kaemu kaliber 50"` | `Naciśnij [%s], aby użyć kaemu kaliber 50` |
| `USEDP28` | `"Naciśnij [%s], aby użyć DP28"` | `Naciśnij [%s], aby użyć DP28` |
| `USESG43` | `"Naciśnij [%s], aby użyć SG43"` | `Naciśnij [%s], aby użyć SG43` |
| `USE30CAL` | `"Naciśnij [%s], aby użyć kaemu kaliber 30"` | `Naciśnij [%s], aby użyć kaemu kaliber 30` |
| `USEMG34` | `"Naciśnij [%s], aby użyć MG34"` | `Naciśnij [%s], aby użyć MG34` |
| `FLAG_INWATER` | `"%s upuścił flagę do wody. ` | `%s upuścił flagę do wody. ` |
| `HINTSTR_GET_DOCUMENTS` | `"Naciśnij [%s], aby zabrać ważne dokumenty.` | `Naciśnij [%s], aby zabrać ważne dokumenty.` |
| `COMMISSAR_ENDING` | `... z radzieckiej ziemi!"` | `... z radzieckiej ziemi!` |
| `HURTGEN_SPAWN_ATTAKER` | `... le camp allié."` | `... le camp allié.` |

## Validation

- Dry run reported exactly the expected entries:
  `pakuo00` (`gmi_cgame.str`, `gmi_ctf.str`, `gmi_script.str`,
  `pi_ponyri.str`), `pakuo01` (the three duplicated files) and
  `localized_french_pak2.pk3` (`re_obj.str`), in both the installation and the
  repaired `_zzzz` tree.
- Archive hashes after `--apply --backup`:

```
localized_french_pak2.pk3      5560186a5e7f9847c1468b3c4f21c55df9089a09d220223657139cdb15e86964
                            -> 6761c4a594c7cf61ad897f3288dc4dd1017ca6b4d1333d0ca9e817751498f995
localized_polish_pakuo00.pk3   b676f8be0f628ef2ffa0c8da31607e6ed3035cb10760e5ca03e56bb3b9f7993e
                            -> f734243a0e804ebc93b5a77a5e8ac4dfe6ab4dcd2eaec9b0babe5240cdb59dd9
localized_polish_pakuo01.pk3   43615aead52456c0b28d2434cba4cd41d22194e8450fdc34a4db086741e7f1a5
                            -> 5ca6136f1e234e8d7c70bee970d8463e84b2f094b3a48a7f062bff75ee75b45e
```

  (`pakuo00`/`pakuo01` already carried the earlier use-key token fix, so their
  "before" values are the post-token-fix archives. Rebuilding the whole chain
  from the pristine release reproduces the same final hashes.)
- Re-running the fix reports `no archive needed a change`, so it is idempotent.
- Re-running the display model over every entry returns the corrected text for
  all nine references and no other extra quotes.
- Started the repaired client with `+map mp_foy`, `developer 1` and
  `logfile 1` for 45 seconds: `localized_polish_pakuo00.pk3 (1299 files)`,
  `localized_polish_pakuo01.pk3 (41 files)` and
  `localized_polish_pak1.pk3 (3758 files)` still load, and the log contains no
  parser, string, or localization error.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/10_localized_stray_quotes.md`
