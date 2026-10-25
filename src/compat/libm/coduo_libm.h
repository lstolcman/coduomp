/* coduo_libm — reconstruction of the Windows CRT math boundaries used by CoD:UO.
 *
 * The statically linked MSVC 7.1 CRT in the original Windows binaries is the
 * behavioral reference on every host. Native x87 instructions retain the
 * documented operation graphs where supported; other targets use native C
 * arithmetic and host math through the shared wide-math adapters.
 *
 * The owning image establishes the ambient x87 precision. Routines preserve
 * that control word unless their comments specify a temporary adjustment.
 * Explicit float/double stores remain rounding boundaries on every target.
 */
#ifndef CODUO_LIBM_H
#define CODUO_LIBM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MSVC _ftol2: convert to int64 with truncation toward zero.
 *
 * Reconstructed from uo_gamex86.dll 0x2008a1ac (175 call sites — every
 * (int)float-expression cast in the server game module). The CRT does not
 * change the rounding mode to truncate; it converts with the ambient
 * round-to-nearest via FISTP and then corrects by one using the sign of the
 * residual, so the correction is reproduced here rather than replaced with a
 * plain C cast. A C cast would agree on all finite in-range inputs but not on
 * overflow, where the x87 indefinite value is produced instead of UB. */
int64_t CoduoLibm_FloatToInt64(double value);

/* Convenience wrapper for the common 32-bit consumer: takes the low dword of
 * the int64 result, matching callers that use only EAX. */
int32_t CoduoLibm_FloatToInt32(double value);

/* sqrt. Reconstructed from uo_gamex86.dll 0x2008a640 (41 call sites).
 *
 * Runs at the owning image's ambient precision: the CRT's control-word helper
 * (0x2008aa15) preserves the PC field and only forces round-to-nearest and
 * masked exceptions. The native square root retains that precision and
 * narrows to double at the return boundary.
 *
 * Edge cases follow the machine code exactly:
 *   +inf      -> +inf   (FSQRT is skipped)
 *   -0.0      -> -0.0   (FSQRT is skipped)
 *   x < 0     -> indefinite QNaN (domain error), including -inf
 *   NaN       -> quieted NaN
 */
double CoduoLibm_Sqrt(double value);

/* asin / acos. Reconstructed from uo_gamex86.dll 0x2008a700 and 0x2008a0e0.
 *
 * Both compute sqrt((1+x)*(1-x)) and feed it to FPATAN, differing only in
 * operand order. The (1+x)(1-x) factoring is stock and is not interchangeable
 * with 1-x*x. Run at the owning image's ambient precision.
 *
 *   |x| > 1  -> indefinite QNaN (domain error)
 *   x == +1  -> asin +pi/2, acos +0.0
 *   x == -1  -> asin -pi/2, acos pi
 *   NaN      -> quieted
 */
double CoduoLibm_Asin(double value);
double CoduoLibm_Acos(double value);

/* sin / cos. Reconstructed from uo_gamex86.dll 0x2008a2e0 and 0x2008a230.
 *
 * The only CRT routines that change precision control: both force CW 0x027F
 * (PC = 53-bit) for the computation and restore on exit. If FSIN/FCOS set C2
 * (|x| >= 2^63) the argument is reduced by FPREM1 against pi*2^62 -- an exact
 * multiple of 2*pi -- and retried.
 *
 *   +-inf -> indefinite QNaN (domain error)
 *   NaN   -> quieted
 */
double CoduoLibm_Sin(double value);
double CoduoLibm_Cos(double value);

/* Native arithmetic limits: an instruction adapter cannot preserve extended
 * storage on a compiler whose long double is binary64. Non-x87 hosts use their
 * native math library for unsupported operations. Neither path uses a software
 * x87 backend; instruction-result fidelity is established per operation. */

#ifdef __cplusplus
}
#endif

#endif /* CODUO_LIBM_H */
