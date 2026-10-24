#include "q_math.h"

#include "compat/coduo_x87emu.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

#if EMULATE_X87
#define MATRIX34_DOT3(a, b, c, d, e, f) \
    x87f_store_f32(x87f_add( \
        x87f_add(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)), \
                 x87f_mul(x87f_load_f32(c), x87f_load_f32(d))), \
        x87f_mul(x87f_load_f32(e), x87f_load_f32(f))))
#define MATRIX34_DOT4(a, b, c, d, e, f, translation) \
    x87f_store_f32(x87f_add( \
        x87f_add(x87f_add(x87f_mul(x87f_load_f32(a), x87f_load_f32(b)), \
                          x87f_mul(x87f_load_f32(c), x87f_load_f32(d))), \
                 x87f_mul(x87f_load_f32(e), x87f_load_f32(f))), \
        x87f_load_f32(translation)))
#else
#define MATRIX34_DOT3(a, b, c, d, e, f) \
    (((WIDE_FLOAT)(a) * (WIDE_FLOAT)(b) + (WIDE_FLOAT)(c) * (WIDE_FLOAT)(d)) \
     + (WIDE_FLOAT)(e) * (WIDE_FLOAT)(f))
#define MATRIX34_DOT4(a, b, c, d, e, f, translation) \
    (MATRIX34_DOT3(a, b, c, d, e, f) + (WIDE_FLOAT)(translation))
#endif

/*
 * The complete 725-byte Linux bodies are instruction-identical:
 *
 *   coduo_lnxded            0x08067d3b
 *   game.mp.uo.i386.so      RVA 0x0003b947
 *
 * Each output is stored as binary32.  The first three columns retain a
 * three-product x87 chain through the store; the translation column adds the
 * left row's fourth component to that same unspilled chain.
 * The Windows game body at RVA 0x177f0 has the same stores but a different
 * product order. Keep those target-specific orders and use WIDE_FLOAT until
 * the output assignment so native non-x87 evaluation does not round each
 * intermediate to binary32. Stores remain progressive when buffers overlap.
 */
void MatrixMultiply34(const float left[3][4], const float right[3][4],
                      float output[3][4])
{
#if defined(WINDOWS_BEHAVIOR)
    output[0][0] = MATRIX34_DOT3(left[0][0], right[0][0], left[0][1], right[1][0], left[0][2], right[2][0]);
    output[0][1] = MATRIX34_DOT3(left[0][1], right[1][1], right[0][1], left[0][0], right[2][1], left[0][2]);
    output[0][2] = MATRIX34_DOT3(left[0][1], right[1][2], right[0][2], left[0][0], right[2][2], left[0][2]);
    output[0][3] = MATRIX34_DOT4(left[0][1], right[1][3], right[0][3], left[0][0], right[2][3], left[0][2], left[0][3]);
    output[1][0] = MATRIX34_DOT3(left[1][2], right[2][0], left[1][1], right[1][0], left[1][0], right[0][0]);
    output[1][1] = MATRIX34_DOT3(left[1][2], right[2][1], left[1][1], right[1][1], left[1][0], right[0][1]);
    output[1][2] = MATRIX34_DOT3(left[1][2], right[2][2], left[1][1], right[1][2], left[1][0], right[0][2]);
    output[1][3] = MATRIX34_DOT4(right[2][3], left[1][2], left[1][1], right[1][3], left[1][0], right[0][3], left[1][3]);
    output[2][0] = MATRIX34_DOT3(left[2][2], right[2][0], left[2][1], right[1][0], right[0][0], left[2][0]);
    output[2][1] = MATRIX34_DOT3(left[2][2], right[2][1], left[2][1], right[1][1], right[0][1], left[2][0]);
    output[2][2] = MATRIX34_DOT3(left[2][2], right[2][2], left[2][1], right[1][2], right[0][2], left[2][0]);
    output[2][3] = MATRIX34_DOT4(left[2][2], right[2][3], left[2][1], right[1][3], right[0][3], left[2][0], left[2][3]);
#else
    for (int row = 0; row < 3; ++row) {
        output[row][0] = MATRIX34_DOT3(
            left[row][0], right[0][0], left[row][1], right[1][0], left[row][2], right[2][0]);
        output[row][1] = MATRIX34_DOT3(
            left[row][0], right[0][1], left[row][1], right[1][1], left[row][2], right[2][1]);
        output[row][2] = MATRIX34_DOT3(
            left[row][0], right[0][2], left[row][1], right[1][2], left[row][2], right[2][2]);
        output[row][3] = MATRIX34_DOT4(
            left[row][0], right[0][3], left[row][1], right[1][3], left[row][2], right[2][3], left[row][3]);
    }
#endif
}

#undef MATRIX34_DOT3
#undef MATRIX34_DOT4
