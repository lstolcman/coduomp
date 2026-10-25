# Stock and Master Branch Policy

This repository has two maintained public products:

- `stock`: a hardened stock-compatible baseline; and
- `master`: the customized/improved product, based on `stock`.

## Stock boundary

Stock preserves behavior for legitimate supported inputs, including gameplay,
presentation, defaults, network and file formats, VM and module interfaces,
ordering, arithmetic, precision, and valid-input failure behavior. The original
Windows i386 client and server binaries define the behavior contract on every
supported host. Linux and macOS builds implement that same contract; original
Linux binaries are not an alternate authority. Platform-behavior selectors and
software x87 emulation are unsupported. Native x87 and wide native C arithmetic
retain the required rounding and truncation boundaries.

Stock also carries implementation changes required to realize that contract on
supported systems:

- shared security hardening;
- pointer-width and calling-convention adaptations for wider hosts;
- required platform and library compatibility layers; and
- narrowly scoped platform adaptations when the original route is unavailable
  or impractical and the replacement preserves observable results.

A hardening check may reject malformed input, but it must not reject legitimate
stock maps, mods, packages, servers, or player behavior. Treat a false positive
as an unresolved bug in the check.

### CRT compatibility boundary

The original Windows CRT functions reached by recovered engine and module code
are part of the behavior contract. Reimplement their observable semantics in
the shared compatibility layer when host libc/libm does not preserve them.
This includes random sequences and image/thread state ownership, parsing and
conversion results, sorting order, character handling, formatting bytes and
return values, and ownership or failure behavior observed by callers. A standard
C name or matching signature does not establish compatibility.

Host services may implement a boundary only when their behavior matches over
the supported caller domain. Private heap layouts, startup machinery, locale
tables, stream internals, exception dispatch, stack probes, and security-cookie
implementations need not be cloned merely because they were statically linked.
Their observable effects still matter: for example, allocation ownership,
environment-string lifetime, and the executable's floating-point initialization.
CRT classification identifies implementation ownership; it does not exempt a
function from behavior recovery. Shared replacements belong in compatibility
code, rather than being presented as original game functions.

Software x87 emulation remains prohibited. Use native x87 where supported and
the widest appropriate native C arithmetic elsewhere, preserving the actual
rounding and truncation points. macOS arm64 cannot reproduce the hardware x87
execution model with its native arithmetic; document the specific limitations
instead of promising identical results. This exception does not cover other
CRT semantics that can be reproduced. In particular, the original decimal
parser's integer arithmetic and construction of floating-point result bits are
CRT algorithms, not an x87 emulator, and remain part of the compatibility layer.

A partial wrapper or host-libm fallback is not proof of complete CRT parity.
Keep unsupported formats, domains, and observable side effects explicit until
verified against the identified original Windows implementation. Required CRT
compatibility changes belong in stock and must be merged forward into master.

## Improved boundary

Improved descends from stock and contains intentional product changes:

- ordinary fixes for original-game bugs that are not shared hardening;
- features and expanded mod support;
- presentation, branding, UI, and usability changes;
- changed defaults, limits, policies, or diagnostics;
- optional compatibility behavior; and
- general performance optimizations.

The `stock..master` diff is the maintained description of customization.
Do not recreate a mixed source tree with `STRICT_STOCK`, another preprocessor
selector, a make variable, or a runtime switch.

## Transcription bugfix workflow

Recovered-source transcription corrections proven against the original binary
restore stock behavior and belong in both branches.

For every transcription bug fix:

1. Implement, validate, and commit the change on `stock` first.
2. Merge `stock` into `master`, preserving the stock commit's ancestry.
3. Validate the merged result on `master`, including `make policy-check` in
   both worktrees, and complete both branches in the same work cycle.

Use a forward merge rather than duplicate cherry-picks or independently
reapplying the patch. A transcription correction is incomplete while present
on only one branch. Keep intentional master-only behavior out of `stock`.

## Public explanations

Keep public comments useful to developers. State exact invariants, bounds,
ownership, sequencing, ABI requirements, portability decisions, and failure
semantics. Preserve `NOT_FROM_ORIGINAL_SOURCE` provenance where applicable.

Keep sensitive failure discovery and exploitation details out of public source,
issues, and commit messages. Do not publish private report paths, exploit
recipes, severity assessments, vulnerable-retail trigger comparisons, or
labels such as `SECURITY_PATCH`. Use a private maintainer channel for those
details and express the public rule directly.

Run `make policy-check` on both branches before release.
