#ifndef CODUO_ASIN_COMPAT_H
#define CODUO_ASIN_COMPAT_H

#include "compat/coduo_x87_eval.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: retain the live result of the CRT inverse-trig
 * entries for float-derived operands. The existing double-return APIs
 * remain the boundary for callers that actually store binary64 results.
 * Domain returns preserve NaN spelling and EDOM. Full OS exception dispatch
 * and nondefault x87 status handling remain outside this adapter; non-x87
 * targets use native math for finite results rather than emulating x87. */
WIDE_FLOAT coduomp_crt_asinl(double value);
WIDE_FLOAT coduomp_crt_acosl(double value);

#ifdef __cplusplus
}
#endif

#endif
