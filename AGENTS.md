# Public Repository Instructions

## Branch contract

- The original Windows client and server binaries are the sole behavioral
  authority on every supported host. Linux and macOS remain supported host
  platforms; their original binaries do not define alternate behavior.
- Do not add platform-behavior selectors or software x87 emulation. Use native
  x87 where available and the widest appropriate native C types elsewhere.
  Preserve `WIDE_FLOAT` intermediates and explicit float/double rounding and
  integer truncation boundaries; removing emulation does not justify narrowing
  live arithmetic to float.
- Original Windows CRT behavior is part of the stock contract. Reimplement
  observable CRT semantics in the shared compatibility layer wherever the host
  library does not preserve them. Matching a standard function name is not
  proof: check results, bytes, ordering, state ownership, and side effects over
  the supported caller domain. Keep host runtime internals only behind that
  boundary; see `docs/branch-policy.md` for the CRT scope.
- Native floating-point limits do not waive reproducible CRT behavior. Preserve
  the integer-based decimal converters and other portable compatibility code;
  these are not software x87 emulation. Document unavoidable native precision
  limits per operation, including on macOS arm64, without narrowing unrelated
  arithmetic or accepting unrelated host-CRT differences.
- Read `docs/branch-policy.md` before changing behavior or moving code between
  branches.
- `stock` is the hardened stock-compatible baseline. Preserve legitimate
  original gameplay, presentation, defaults, protocols, formats, ordering,
  arithmetic, precision, and supported-content behavior.
- `stock` may contain shared hardening, required 64-bit and ABI adaptations,
  required compatibility layers, and platform-local adaptations that preserve
  observable results.
- `master` descends from `stock` and owns the improved product's intentional
  non-stock behavior: ordinary original-game bug fixes, features, changed
  defaults or limits, presentation changes, optional compatibility, and general
  runtime optimizations.
- Build tooling must be kept the same on `stock` and `master`: Make/CMake
  mechanisms, parallelism and other build defaults, toolchain and dependency
  handling, packaging, build metadata, and documentation for shared build
  behavior. Build-performance improvements belong in both branches. Preserve
  each branch's intended product settings and source membership when sharing
  tooling. Implement, validate, and commit tooling changes on `stock` first,
  then merge `stock` forward into `master` and validate both in the same work
  cycle. Tooling work is incomplete while present on only one branch; it is
  not covered by the master-only runtime optimization rule.
- Make changes in the correct concrete branch. Never add `STRICT_STOCK` or an
  equivalent compile-time or runtime selector that recombines the branches.
- For transcription bug fixes proven against the original binary, implement,
  validate, and commit on `stock` first. Then merge `stock` into `master` and
  validate there in the same work cycle. Preserve ancestry rather than
  duplicating commits; follow the transcription bugfix workflow in
  `docs/branch-policy.md`.

## Security and provenance

- Shared security hardening belongs in both branches. A stock false positive
  against legitimate supported content is a bug and must be resolved before
  release.
- Public comments should state the maintained invariant directly: bounds,
  ownership, type or ABI requirements, sequencing, and failure behavior.
- Do not add exploit recipes, severity assessments, private report paths,
  vulnerable-retail trigger comparisons, or labels such as `SECURITY_PATCH`.
  Coordinate sensitive details with maintainers through a private channel.
- Preserve `NOT_FROM_ORIGINAL_SOURCE` on non-original helpers and adaptations.
  New non-original functions must carry that marker and use a lower-case
  project, compatibility, or adaptation namespace such as `coduomp_*`.
- Do not add private analysis records, retail binaries, generated decompiler
  output, source-recovery ledgers, or machine-specific workspace paths.

## Change discipline

- This is a maintained product repository, not a scratchboard. Do not create
  or commit plans, proposed designs, task notes, audit write-ups, or working
  documents here unless the user explicitly requests that document here.
- Requests to evaluate, discuss, plan, or "design first" belong in the
  conversation by default. Naming this repository as an implementation target
  does not authorize adding design documents. Keep optional draft files in the
  designated private analysis workspace, not in this public tree.
- Public product documentation should describe implemented, supported behavior
  unless the user explicitly requests otherwise.
- Do not delete an existing code comment unless the complete code section it
  describes is also removed. Rewrite an inaccurate comment in place.
- Keep nearby C/C++ formatting; do not mechanically hard-wrap unaffected code.
- Put generated builds, packages, logs, caches, and disposable runtime state
  under the repository-root `.workbench/`; do not create ignored output beside
  maintained source.
- Release tooling must not embed a maintainer's username, home directory,
  hostname, workspace name, network address, or private remote path. Keep
  machine-specific release settings as invocation-time values and run the
  maintained package privacy/import audits.
- Validate source-policy invariants with `make policy-check` before committing.
- Build and test every affected supported target in proportion to the change.
- `client-run` is the user-facing normal-game launcher and intentionally uses
  the persistent system profile. Agents must never invoke it or create, edit,
  move, seed, restore, replace, or delete files in the profile it uses unless
  the user explicitly requests the exact operation and identifies the
  authoritative source for any restored or replacement data.
- Do not edit, refactor, rename, restructure, or otherwise change `client-run`,
  `client-test-run`, their dependency graph, or their
  configuration/profile/home-path routing unless the user explicitly requests
  that exact change.
