#ifndef CGAME_NATIVE_POWER_H
#define CGAME_NATIVE_POWER_H

#include <math.h>

/* NOT_FROM_ORIGINAL_SOURCE: isolated icon fading uses host power, accepting its
 * result and error behavior. Gameplay and world geometry keep CRT fidelity.
 * Retain native long-double operands/results where supported. The Microsoft CRT has a binary64 ABI even when clang-cl uses an
 * 80-bit source carrier, so cross that boundary explicitly through pow.
 * Callers retain their surrounding wide arithmetic and explicit stores. The
 * aliases add no wrapper call, and each argument is evaluated exactly once. */
#if defined(_MSC_VER)
#define cgame_compat_native_powl(base, exponent) ((long double)pow((double)(base), (double)(exponent)))
#else
#define cgame_compat_native_powl(base, exponent) powl((base), (exponent))
#endif

#endif
