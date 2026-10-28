# Bundled libraries are pinned to release commits, not snapshots

## Report

> fix libs to specific commits not snapshot. for reproducibility!

## Investigation

`libs/CMakeLists.txt` already downloads every bundled dependency by commit
URL and verifies the archive with a SHA-256 hash. Nothing in the file fetched a
branch head, a tag ref, or an unversioned "snapshot" archive:

| Library | URL commit | SHA-256 |
| --- | --- | --- |
| zlib | `da607da739fa6047df13e66a2af6b8bec7c2a498` | `21DE0017...5EC068F8` |
| SDL2 | `c98c4fbff6d8f3016a3ce6685bf8f43433c3efcc` | `516E788F...9B1068FA` |
| libjpeg-turbo | `e352b02f794f701407b39af08576035ba3360d60` | `BAC5A0E6...2329D6F8` |
| curl | `8c908d2d0a6d32abdedda2c52e90bd56ec76c24d` | `084E07B0...E24FE753` |
| Khronos headers | commit-addressed `raw.githubusercontent.com` URLs | per-file SHA-256 |

The comments were misleading, however:

- the header said SDL2 came from the "SDL2 branch";
- curl was labelled an "8.19.0 development snapshot", and the downloaded tree
  really does contain `LIBCURL_VERSION "8.19.0-DEV"` in `curlver.h`;
- the per-library comments named only versions, not the release tags the
  commits belong to.

That reads as if a moving development tree were being fetched, even though the
build was reproducible.

## Verification

Each pinned commit was resolved through the GitHub API and compared with the
release tag ref:

| Tag | Tag commit | Pinned commit |
| --- | --- | --- |
| `curl-8_19_0` | `8c908d2d0a6d32abdedda2c52e90bd56ec76c24d` | same |
| `v1.3.2` (zlib) | `da607da739fa6047df13e66a2af6b8bec7c2a498` | same |
| `release-2.30.9` (SDL) | `c98c4fbff6d8f3016a3ce6685bf8f43433c3efcc` | same |
| `3.1.4` (libjpeg-turbo) | `e352b02f794f701407b39af08576035ba3360d60` | same |

curl's release commit still carries the `-DEV` version string because upstream
does not rewrite `curlver.h` inside the release tag; the commit is the tagged
release commit, not a later development commit. The Khronos entries were
already commit-addressed and hash-verified.

## Fix

Comment-only correction so the build declares what it actually does:

- the header now states that no branch, tag ref, or moving snapshot is
  downloaded, and lists the release tag beside each dependency;
- each fetch comment names the version and the release tag;
- the curl comment explains why the in-tree version string ends in `-DEV`.

No URL, commit, hash, or build input changed. Existing build trees and
downloaded archives stay valid; the reproducibility properties were already
correct. Re-auditing a pin means resolving its release tag to a commit and
comparing the result with the URL, as in the table above.

## Validation

- `git diff` shows only comments in `libs/CMakeLists.txt`.
- The four archive hashes remain verified by `FetchContent`'s
  `URL_HASH SHA256=...` on every fresh configure.
- `make policy-check` passes.
- No rebuild is required to keep an existing build valid; a fresh configure
  downloads the same archives as before.

## Files

- `libs/CMakeLists.txt`
- `docs/bugfixes/08_dependency_pinning.md`
