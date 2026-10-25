#ifndef CODUO_X87_EVAL_H
#define CODUO_X87_EVAL_H

/* NOT_FROM_ORIGINAL_SOURCE: carrier for a live x87 intermediate, never a fixed
 * external ABI or stored-data layout type. Internal call boundaries may use it
 * with matching declarations and consumers. Use long double to retain native
 * x87 precision where available and the widest standard native floating-point
 * type elsewhere. Explicit float and double rounding points remain in the
 * arithmetic body; selecting float as the carrier can narrow too early. */
#define WIDE_FLOAT long double

/* NOT_FROM_ORIGINAL_SOURCE: explicit binary32 rounding inside a wide
 * expression. The volatile object prevents an excess-precision value from
 * surviving the store; the result remains convenient for wide evaluation. */
static inline WIDE_FLOAT coduo_round_f32(WIDE_FLOAT value)
{
    volatile float rounded = value;
    return rounded;
}

#endif /* CODUO_X87_EVAL_H */
