#include "q_math.h"



/*
 * The four authoritative Windows bodies are byte-identical:
 *
 *   CoDUOMP.exe                 0x004333c0
 *   uo_cgame_mp_x86.dll        0x3004b520
 *   uo_ui_mp_x86.dll           0x400034f0
 *   uo_game_mp_x86.dll         0x20018570
 *
 * Windows folds the X output in Y,Z,X order and the remaining outputs in
 * X,Y,Z order under the process PC=53 policy. The two authoritative Linux
 * bodies are likewise byte-identical at coduo_lnxded 0x080692a8 and
 * game.mp.uo.i386.so RVA 0x0003ceeb, but fold every output in X,Y,Z order
 * under PC=64. Each output is stored to binary32 before the next lane begins,
 * preserving the original progressive behavior when input and output overlap.
 *
 * The supporting Mac cgame and game modules retain the exact
 * MatrixTransformVector43 name and the compact 0x30-byte layout.
 */
void MatrixTransformVector43(const vec3_t input,
                             const matrix43_t *matrix, vec3_t output)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds 12(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "flds 24(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 36(%1)\n\t"
                         "fstps 0(%2)\n\t"
                         "flds 4(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 16(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 28(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 40(%1)\n\t"
                         "fstps 4(%2)\n\t"
                         "flds 8(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 20(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 32(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fadds 44(%1)\n\t"
                         "fstps 8(%2)"
                         :
                         : "r"(input), "r"(matrix), "r"(output)
                         : "st", "st(1)", "memory");
#else
    output[0] = (float)((((double)matrix->axis[1][0] * (double)input[1] +
                          (double)matrix->axis[2][0] * (double)input[2]) +
                         (double)matrix->axis[0][0] * (double)input[0]) +
                        (double)matrix->origin[0]);
    output[1] = (float)((((double)matrix->axis[0][1] * (double)input[0] +
                          (double)matrix->axis[1][1] * (double)input[1]) +
                         (double)matrix->axis[2][1] * (double)input[2]) +
                        (double)matrix->origin[1]);
    output[2] = (float)((((double)matrix->axis[0][2] * (double)input[0] +
                          (double)matrix->axis[1][2] * (double)input[1]) +
                         (double)matrix->axis[2][2] * (double)input[2]) +
                        (double)matrix->origin[2]);
#endif
}
