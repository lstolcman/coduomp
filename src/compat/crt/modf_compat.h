#ifndef CODUOMP_CRT_MODF_COMPAT_H
#define CODUOMP_CRT_MODF_COMPAT_H

#ifdef __cplusplus
extern "C" {
#endif

/* NOT_FROM_ORIGINAL_SOURCE: shared Windows CRT binary64 split boundary.
 * NaNs are quieted with their payload intact and normally report EDOM; the
 * SSE2 denormals-are-zero mode retains its signed-zero fraction exception. */
double coduomp_crt_modf(double value, double *whole);

#ifdef __cplusplus
}
#endif

#endif
