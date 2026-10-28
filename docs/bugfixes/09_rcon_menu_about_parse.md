# The bundled rcon menu's About page fails to parse

## Symptom

Every multiplayer client start and every map load prints four red menu
errors:

```
^1Error: file ui_mp/scriptmenus/about.menu, line 143: newline inside string "Special thanks to mnml *
^1Menu load error: ui_mp/scriptmenus/about.menu, line 143: couldn't parse menu item keyword text
^1Menu load error: ui_mp/scriptmenus/about.menu, line 143: couldn't parse menu keyword itemDef
^1Error: file ui_mp/scriptmenus/about.menu, line 144: newline inside string "
```

The same four lines appear again when a map is loaded, because the ingame
menu is rebuilt. The `itemDef` that carries the "Special thanks" line is
discarded and the rest of the page is parsed from the wrong token.

## Investigation

`ui_mp/scriptmenus/about.menu` is shipped inside `zzz_rconmenu.pk3`. The
archive is loaded by `ui_mp/menus.txt`, and `ui_mp/ingame.txt` loads the About
page at ingame-menu init:

```
loadMenu { "ui_mp/scriptmenus/about.menu" }
```

At file offset 2528 the item reads:

```
			textaligny		12
			text			"Special thanks to mnml *
"
			decoration
```

The bytes of the value are:

```
74 65 78 74 09 09 09 22   text...
53 70 65 63 69 61 6c ...  "Special thanks to mnml *
0d 0a 22                  CR LF "
```

The quoted string therefore contains a literal CR LF. The menu tokenizer
scans a quoted string to the next quote and rejects a raw line break inside
it, so the value cannot be closed on line 143. The parser reports the
newline, then tries to read the token `"` on line 144 as the next keyword and
reports a second newline error. Everything after that point in the file is
still read, but the credits item is gone.

The same archive is distributed twice, in `Main/` and in `uo/`, with
identical bytes (SHA-256 `6af8899a0a71c62e9d0eec0647cc84d5512c40a6647a32617a174e4add1b1b3e`).
It is present in the Polish 1.51b installation and in the Polish reference
tree; the English reference tree does not ship this archive at all. Both
copies of the archive contain the same defect, and so does the copy in the
repaired `_zzzz` tree.

A second, harmless oddity in the same archive is a menu-button definition
stored as `ui_mp/scriptmenus/Neu Textdokument.txt`. It is the only file that
executes `open about;`, but its `.txt` extension means the menu loader never
reads it. The About page therefore exists only as a loaded menu without a
button that opens it. That is a packaging mistake in the bundled modification,
not a parser defect; this fix does not invent a new ingame button and leaves
the file untouched.

## Fix

`tools/patch-coduo-game-data.py` gains a third game-data fix, `rconmenu`. It
rewrites the single line

```
			text			"Special thanks to mnml *\r\n"
```

to

```
			text			"Special thanks to mnml *"
```

only inside `ui_mp/scriptmenus/about.menu`, and only when that byte sequence
occurs exactly once. The archive is then re-read and CRC-checked before it
replaces the original, and `.bugfix-backup` copies are written when
`--backup` is given. As with the other fixes, the tool is a dry run unless
`--apply` is passed:

```
tools/patch-coduo-game-data.py --game-root /path/to/coduo
tools/patch-coduo-game-data.py --game-root /path/to/coduo --fix rconmenu --apply --backup
```

## Validation

- Reproduced on the repair tree before the patch: the errors appear at log
  lines 146, 148, 149, 150 during startup and at lines 580, 582, 583, 584
  after `+map mp_foy`.
- Dry run reported exactly one `about.menu` change in each of the four
  archives (the `Main/` and `uo/` copies in the installation and in `_zzzz`).
- After `--apply --backup`:
  `6af8899a0a71c62e9d0eec0647cc84d5512c40a6647a32617a174e4add1b1b3e`
  became `398a2cfe6bb1f7881f1772d80a4355b7962df630d380231b8eb085eca4759743`
  in all four copies.
- Re-ran the client with `+map mp_foy`, `developer 1`, and `logfile 1` for
  45 seconds: the log still loads the archive (`zzz_rconmenu.pk3 (31 files)`)
  and contains no `about.menu`, `newline inside string`, or
  `Menu load error` line.
- `make policy-check` passes.

## Files

- `tools/patch-coduo-game-data.py`
- `docs/bugfixes/09_rcon_menu_about_parse.md`
