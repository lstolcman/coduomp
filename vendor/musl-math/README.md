# Extended-precision math slice

Unmodified files from musl 1.2.5 supply the standard long-double functions
needed by the clang-cl x64 native-x87 build. `upstream.json` records the source
archive, original paths, and file hashes; `COPYRIGHT` and the source headers
retain the upstream license notices.

Only `cmake/CoduompExtendedMath.cmake` builds this slice. Its compatibility
headers namespace the symbols and supply the missing Windows header glue.
The library uses 80-bit `long double`, source-type evaluation, strict floating
point, and no contraction. Product code also uses source-type evaluation:
explicit `WIDE_FLOAT`/`long double` operations use x87; ordinary `float` and
`double` expressions retain SSE evaluation. The entry adapters preserve the caller's control word and select
64-bit significand precision for the polynomial kernels. Square roots use the
existing native FSQRT helper at the caller's precision.

This supplies an extended-precision library ABI where the Microsoft CRT only
offers binary64. It does not replace the reconstructed `CoduoLibm` routines or
claim that unreconstructed standard-library calls match the retail CRT.
