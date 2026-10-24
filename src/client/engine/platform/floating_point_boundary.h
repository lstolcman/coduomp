#ifndef CODUOMP_FLOATING_POINT_BOUNDARY_H
#define CODUOMP_FLOATING_POINT_BOUNDARY_H

#if defined(_WIN32)
#if defined(_MSC_VER) && defined(_M_X64)
#if !defined(EMULATE_X87) || !EMULATE_X87
#error "MSVC x64 requires the explicitly selected emulated floating-point build"
#endif
/* NOT_FROM_ORIGINAL_SOURCE: MSVC x64 emits SSE arithmetic and has no native
 * x87 precision-control path. Emulated sites own their SoftFloat precision;
 * the remaining native arithmetic is covered by the relaxed build policy. */
#define CODUOMP_NATIVE_X87_PRECISION_CONTROL 0
#define coduomp_restore_retail_x87_precision() ((void)0)
#else
#define CODUOMP_NATIVE_X87_PRECISION_CONTROL 1
void coduomp_restore_retail_x87_precision(void);
#endif
#else
#define CODUOMP_NATIVE_X87_PRECISION_CONTROL 0
#endif

#endif
