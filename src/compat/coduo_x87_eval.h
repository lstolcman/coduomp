#ifndef CODUO_X87_EVAL_H
#define CODUO_X87_EVAL_H

/* NOT_FROM_ORIGINAL_SOURCE: native x87 evaluation carrier, never an ABI or
 * stored-data type. Modern x87 builds use long double so compiler spills keep
 * the register format. An optimized x87 compiler configuration verified to
 * retain float intermediates may select CODUO_X87_EVAL_TYPE=float; the original
 * Win32 compiler uses this setting. Binary32 rounding points still use float,
 * and non-x87 targets continue to use the software-x87 implementation. */
#ifndef CODUO_X87_EVAL_TYPE
#define CODUO_X87_EVAL_TYPE long double
#endif

#endif /* CODUO_X87_EVAL_H */
