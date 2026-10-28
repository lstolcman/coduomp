# Korean and Taiwanese messages lose their format marker

## Symptom

Korean players see a bare percent sign in the vote and timeout messages, with
the value that belongs inside the sentence appended after it:

```
%가 타임아웃을 요청했습니다.Axis
%는 이미 타임아웃을 모두 사용하였습니다.
킬캠: %MENU_OFF
투표 %를 위한 유효한 논쟁이 없습니다ban
```

The Taiwanese server-restart notice has the same defect:

```
由於% 因素，重新啟動伺服器numSnapshotEntities
```

Instead of replacing the marker, the message prints `%` and appends the
argument after the final punctuation.

## Investigation

The server builds these messages by concatenating a localized reference and one
or more following fragments:

```c
va("e \"PATCH_1_5_TIMEOUT_CALLED\x14%s\x15", teamName)
va("e \"PATCH_1_5_TIMEOUT_CALLED_PLAYERNAME\x15%s\"", cleanName)
Com_sprintf(lvl->voteDisplayString, sizeof(lvl->voteDisplayString),
            "PATCH_1_5_VOTE_KILLCAM\x14%s", ...);
va("e \"EXE_SERVERRESTARTMISC\x15" "numSnapshotEntities");
```

`\x14` marks the next fragment as localized, `\x15` marks it as plain text, and
`\x16` is the escaped-percent marker. `SEH_LocalizeTextMessage`
(`src/client/engine/localization/string_ed_api.cpp`) counts only the `%s`
pairs inside a localized fragment. When a following fragment supplies the
value, the first pending `%s` is replaced in place. A lone `%` is not a marker:
`pendingInsertions` stays zero, the following fragment is appended at the end
of the message, and the `%` is printed as written.

The Korean file `localizedstrings/korean/patch_1_5.str` (package `PATCH_1_5`)
stores the marker as a bare `%` followed directly by the Korean particle. The
missing `s` turns `%s가`, `%s는`, `%s를` into `%가`, `%는`, `%를`, and the
sentence-final `%s` in the three vote messages into a trailing `%`.
`localizedstrings/taiwanese/exe.str` (package `EXE`) does the same for
`SERVERRESTARTMISC`.

Call sites that make the entries reachable:

- `src/server/game/client_commands.c`: `PATCH_1_5_TIMEOUT_CALLED`,
  `PATCH_1_5_TIMEOUT_CALLED_PLAYERNAME`, `PATCH_1_5_TIMEOUT_MAXCALLED`,
  `PATCH_1_5_TIMEOUT_MAXTIMEUSED`, `PATCH_1_5_TIMEIN_CALLED_PLAYERNAME`,
  `PATCH_1_5_VOTE_DRAWFRIEND`, `PATCH_1_5_VOTE_KILLCAM`,
  `PATCH_1_5_VOTE_FRIENDLYFIRE`, `PATCH_1_5_VOTE_ARG_NOTVALID`.
- `src/server/engine/server_frame.c`: `EXE_SERVERRESTARTMISC`.

The English text of every affected entry has exactly one `%s` at the same
position, and every call site supplies exactly one fragment. Ten entries in the
two archives are affected:

| Archive | Reference | Before | After |
| --- | --- | --- | --- |
| `localized_korean_pak5.pk3` | `TIMEOUT_CALLED` | `%가 타임아웃을 요청했습니다.` | `%s가 타임아웃을 요청했습니다.` |
| `localized_korean_pak5.pk3` | `TIMEOUT_CALLED_PLAYERNAME` | `%가 타임아웃을 요청했습니다.` | `%s가 타임아웃을 요청했습니다.` |
| `localized_korean_pak5.pk3` | `TIMEOUT_MAXCALLED` | `%는 이미 타임아웃을 모두 사용하였습니다.` | `%s는 이미 타임아웃을 모두 사용하였습니다.` |
| `localized_korean_pak5.pk3` | `TIMEOUT_MAXTIMEUSED` | `%가 사용 가능한 타임아웃을 모두 사용했습니다.` | `%s가 사용 가능한 타임아웃을 모두 사용했습니다.` |
| `localized_korean_pak5.pk3` | `TIMEIN_CALLED_PLAYERNAME` | `%가 타임인을 불렀습니다.` | `%s가 타임인을 불렀습니다.` |
| `localized_korean_pak5.pk3` | `VOTE_DRAWFRIEND` | `우호적 지표들: %` | `우호적 지표들: %s` |
| `localized_korean_pak5.pk3` | `VOTE_KILLCAM` | `킬캠: %` | `킬캠: %s` |
| `localized_korean_pak5.pk3` | `VOTE_FRIENDLYFIRE` | `우호적인 화염: %` | `우호적인 화염: %s` |
| `localized_korean_pak5.pk3` | `VOTE_ARG_NOTVALID` | `투표 %를 위한 유효한 논쟁이 없습니다` | `투표 %s를 위한 유효한 논쟁이 없습니다` |
| `localized_taiwanese_pak2.pk3` | `SERVERRESTARTMISC` | `由於% 因素，重新啟動伺服器` | `由於%s 因素，重新啟動伺服器` |

`TIMEOUT_CALLED` and `TIMEOUT_CALLED_PLAYERNAME` share the same Korean sentence,
so the stored line occurs twice in the file and both copies are fixed.

Both archives were byte-identical to the pristine Polish 1.51b release before
the fix, so the defect is in the shipped translation:

```
localized_korean_pak5.pk3     731122c0b10872f13981ae933e858af24d1f0534eb61607ef657310b488280f7
localized_taiwanese_pak2.pk3  c8dc11fd609ef89b80f76f0e90480f42705805099b99a53428206408185ec342
```

Other placeholder differences in the shipped localization were checked and are
not defects:

- German `SERVERFILTER` stores `#same`. `CStringEdPackage::SetString` copies the
  preceding English text for `#same`, which keeps the English `%s` and the
  matching argument.
- Italian and Japanese `CANTJOINTEAM_RUSSIAN`, `CANTJOINTEAM_BRITISH`, and
  `CANTJOINTEAM_AMERICAN` carry an extra `%s`, but no script references those
  keys. The team-join code only calls `PATCH_1_3_CANTJOINTEAM_ALLIED`,
  `PATCH_1_3_CANTJOINTEAM_ALLIED2`, and `PATCH_1_3_CANTJOINTEAM_AXIS`, whose
  translations match their single argument.
- `%d` and `%i` in `exe.str` belong to the client's own `Com_sprintf`/`va`
  formatting of server counts, not to the message-fragment protocol.

## Fix

`tools/patch-coduo-game-data.py` gains the `percentfix` fix. It carries the ten
exact stored lines with their expected occurrence counts, replaces each one,
and aborts if an archive contains a different number. The archive is re-read,
CRC-checked, the entry list is compared, and the planner is re-run against the
rewritten archive before it replaces the original:

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix percentfix
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix percentfix --apply --backup
```

## Validation

- Dry run reported exactly the two archives in the installation and in the
  repaired `_zzzz` tree, and only the two expected entries.
- Archive hashes after `--apply --backup`:

```
localized_korean_pak5.pk3     731122c0b10872f13981ae933e858af24d1f0534eb61607ef657310b488280f7
                           -> ea3cd38edbdba9d8798cc319081dcc72bb0f910cb0efb61a270fefd90e8a803f
localized_taiwanese_pak2.pk3  c8dc11fd609ef89b80f76f0e90480f42705805099b99a53428206408185ec342
                           -> 798de34754a86caf83c9f1189ab91813b1f34ac904576388a7080d69cf4b1f7e
```

- Re-running the fix reports `no archive needed a change`, so it is idempotent.
- Decoding the patched entries with the Korean (cp949) and Taiwanese (Big5)
  code pages returns the sentences shown in the table, each with a single `%s`
  where the argument belongs.
- A scan of every `.str` value in `Main/` and `uo/` for a percent sign that is
  not followed by `s`, `%`, or a format specifier used by the UI finds only
  these ten entries; after the fix the scan is empty for the message protocol.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/12_korean_taiwanese_format_marker.md`
