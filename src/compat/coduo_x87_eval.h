#ifndef CODUO_X87_EVAL_H
#define CODUO_X87_EVAL_H

/* NOT_FROM_ORIGINAL_SOURCE: carrier for a live x87 intermediate, never an ABI
 * or stored-data type. The original-compiler build profile uses float where
 * its generated code has been verified to retain the live x87 value. Modern
 * builds use long double to retain native x87 precision where available and
 * the widest standard native floating-point type elsewhere. Explicit float
 * and double rounding points remain in the arithmetic body. Select this by
 * compiler profile, not CPU: modern float evaluation can narrow too early. */
#if defined(CODUO_ORIGINAL_COMPILER) && CODUO_ORIGINAL_COMPILER
#define CODUO_X87_EVAL_TYPE float
#else
#define CODUO_X87_EVAL_TYPE long double
#endif

#endif /* CODUO_X87_EVAL_H */
