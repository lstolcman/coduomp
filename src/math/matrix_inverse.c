#include "q_math.h"



/*
 * The authoritative Windows bodies are instruction-identical except for the
 * relocated address of their binary32 1.0 constant:
 *
 *   CoDUOMP.exe                 0x00432b60
 *   uo_cgame_mp_x86.dll        0x3004acc0
 *   uo_ui_mp_x86.dll           0x40002c90
 *   uo_game_mp_x86.dll         0x20017d10
 *
 * The Linux engine and game-module bodies at 0x080688ac and RVA 0x0003c4b8
 * retain the same adjugate and determinant under their PC=64 operation graph.
 * Windows stores the first cofactor to binary32 before using it in the
 * determinant; Linux keeps that minor live until its determinant term is
 * formed.  That boundary, rather than the engine's FLD1 versus the PIC game
 * module's load of the same exact binary32 1.0, requires the behavior split.
 * The supporting Mac executable retains the canonical MatrixInverse symbol at
 * PEF address 0x000dc390, but uses fused binary32 PowerPC arithmetic and
 * snapshots every input lane before output; it is corroborating rather than
 * authoritative behavior.
 *
 * Both x86 families intentionally lack a singular-matrix guard.  They also
 * reload source lanes after earlier destination stores, so partially or fully
 * overlapping matrices retain the original progressive overwrite behavior.
 */
void MatrixInverse(axis_t input, axis_t output)
{
    /* Windows stores this first cofactor to binary32 before using it in both
     * the determinant and output[0][0]. */
    const float cofactor00 = (float)(
        (double)input[1][1] * (double)input[2][2] -
        (double)input[2][1] * (double)input[1][2]);
    const float determinant = (float)(
        ((double)cofactor00 * (double)input[0][0] -
         ((double)input[0][1] * (double)input[2][2] -
          (double)input[2][1] * (double)input[0][2]) *
             (double)input[1][0]) +
        ((double)input[0][1] * (double)input[1][2] -
         (double)input[0][2] * (double)input[1][1]) *
            (double)input[2][0]);
    const float inverseDeterminant =
        (float)((double)1.0f / (double)determinant);

    output[0][0] = (float)((double)cofactor00 * inverseDeterminant);
    output[0][1] = (float)(-
        (((double)input[0][1] * (double)input[2][2] -
          (double)input[2][1] * (double)input[0][2]) *
         inverseDeterminant));
    output[0][2] = (float)(
        ((double)input[0][1] * (double)input[1][2] -
         (double)input[0][2] * (double)input[1][1]) *
        inverseDeterminant);
    output[1][0] = (float)(-
        (((double)input[1][0] * (double)input[2][2] -
          (double)input[2][0] * (double)input[1][2]) *
         inverseDeterminant));
    output[1][1] = (float)(
        ((double)input[0][0] * (double)input[2][2] -
         (double)input[0][2] * (double)input[2][0]) *
        inverseDeterminant);
    output[1][2] = (float)(-
        (((double)input[0][0] * (double)input[1][2] -
          (double)input[0][2] * (double)input[1][0]) *
         inverseDeterminant));
    output[2][0] = (float)(
        ((double)input[2][1] * (double)input[1][0] -
         (double)input[2][0] * (double)input[1][1]) *
        inverseDeterminant);
    output[2][1] = (float)(-
        (((double)input[2][1] * (double)input[0][0] -
          (double)input[0][1] * (double)input[2][0]) *
         inverseDeterminant));
    output[2][2] = (float)(
        ((double)input[1][1] * (double)input[0][0] -
         (double)input[0][1] * (double)input[1][0]) *
        inverseDeterminant);
}
