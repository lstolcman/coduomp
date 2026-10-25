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
 * The Windows game body at RVA 0x177f0 has the same output stores. Use
 * WIDE_FLOAT until the output assignment so native non-x87 evaluation does
 * not round each intermediate to binary32. Stores remain progressive when
 * buffers overlap. Windows folds the last two rows in reverse product order;
 * cancellation can change their stored result, so select that order without
 * duplicating the multiplication body.
 */
void MatrixMultiply34(const float left[3][4], const float right[3][4],
                      float output[3][4])
{
    for (int row = 0; row < 3; ++row) {
#if defined(WINDOWS_BEHAVIOR)
        const int first = row == 0 ? 0 : 2;
        const int last = row == 0 ? 2 : 0;
#else
        const int first = 0, last = 2;
#endif
        output[row][0] = MATRIX34_DOT3(
            left[row][first], right[first][0], left[row][1], right[1][0], left[row][last], right[last][0]);
        output[row][1] = MATRIX34_DOT3(
            left[row][first], right[first][1], left[row][1], right[1][1], left[row][last], right[last][1]);
        output[row][2] = MATRIX34_DOT3(
            left[row][first], right[first][2], left[row][1], right[1][2], left[row][last], right[last][2]);
        output[row][3] = MATRIX34_DOT4(
            left[row][first], right[first][3], left[row][1], right[1][3], left[row][last], right[last][3], left[row][3]);
    }
}

#undef MATRIX34_DOT3
#undef MATRIX34_DOT4
