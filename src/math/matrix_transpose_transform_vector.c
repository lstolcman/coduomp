#include "q_math.h"



/*
 * The four authoritative Windows bodies are byte-identical:
 *
 *   CoDUOMP.exe                 0x00433370
 *   uo_cgame_mp_x86.dll        0x3004b4d0
 *   uo_ui_mp_x86.dll           0x400034a0
 *   uo_game_mp_x86.dll         0x20018520
 *
 * UI also retains a byte-identical duplicate at 0x40001a10.  The misleading
 * recovered cgame/UI names around the first cluster were disentangled by the
 * operation graph: the preceding 0x3004b480/0x40003450 bodies transform by
 * matrix columns, while the bodies above perform this row-dot transpose.
 *
 * The two authoritative Linux bodies are byte-identical at coduo_lnxded
 * 0x080691fe and game.mp.uo.i386.so RVA 0x0003ce41.  Windows folds the first
 * row in y,z,x order and the remaining rows in x,y,z order under PC=53. Linux
 * folds every row in x,y,z order under PC=64. Each target writes one binary32
 * lane before reloading operands for the next lane, so the original
 * progressive behavior for overlapping vector, matrix, and output storage is
 * retained.
 *
 * The supporting Mac executable names MatrixTransposeTransformVector at PEF
 * address 0x000d78a0. It corroborates the row-dot operation but uses binary32
 * fused multiply-adds and computes all three lanes before storing any output;
 * it therefore remains supporting rather than authoritative behavior.
 */
void MatrixTransposeTransformVector(const vec3_t vector, axis_t matrix,
                                    vec3_t transformed)
{
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
    __asm__ __volatile__("flds 4(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "flds 8(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 0(%0)\n\t"
                         "fmuls 0(%1)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps 0(%2)\n\t"
                         "flds 12(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 16(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 20(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps 4(%2)\n\t"
                         "flds 24(%1)\n\t"
                         "fmuls 0(%0)\n\t"
                         "flds 28(%1)\n\t"
                         "fmuls 4(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "flds 32(%1)\n\t"
                         "fmuls 8(%0)\n\t"
                         "faddp %%st, %%st(1)\n\t"
                         "fstps 8(%2)"
                         :
                         : "r"(vector), "r"(matrix), "r"(transformed)
                         : "st", "st(1)", "memory");
#else
    transformed[0] = (float)(
        ((double)matrix[0][1] * (double)vector[1] +
         (double)matrix[0][2] * (double)vector[2]) +
        (double)vector[0] * (double)matrix[0][0]);
    transformed[1] = (float)(
        ((double)matrix[1][0] * (double)vector[0] +
         (double)matrix[1][1] * (double)vector[1]) +
        (double)matrix[1][2] * (double)vector[2]);
    transformed[2] = (float)(
        ((double)matrix[2][0] * (double)vector[0] +
         (double)matrix[2][1] * (double)vector[1]) +
        (double)matrix[2][2] * (double)vector[2]);
#endif
}
