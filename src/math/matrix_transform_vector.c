#include "q_math.h"



/*
 * MatrixInverseOrthogonal43 is likewise shared by every image.  Its four
 * Windows bodies are instruction-identical at CoDUOMP.exe 0x00432c70,
 * cgame 0x3004add0, UI 0x40002da0, and game 0x20017e20.  They transpose by
 * nine dword copies and use specialized Y,Z,X / Y,Z,X / Z,X,Y translation-dot
 * orders.  Linux coduo_lnxded 0x08068acc and game RVA 0x0003c6ec instead call
 * MatrixTranspose followed by MatrixTransformVector, giving X,Y,Z order for
 * all three translation lanes.  The complete bodies preserve that genuine
 * source difference and the positive-zero vec3_origin subtractions.
 */
void MatrixInverseOrthogonal43(const matrix43_t *input,
                               matrix43_t *output)
{
    vec3_t negativeTranslation;

    output->axis[0][0] = input->axis[0][0];
    output->axis[0][1] = input->axis[1][0];
    output->axis[0][2] = input->axis[2][0];
    output->axis[1][0] = input->axis[0][1];
    output->axis[1][1] = input->axis[1][1];
    output->axis[1][2] = input->axis[2][1];
    output->axis[2][0] = input->axis[0][2];
    output->axis[2][1] = input->axis[1][2];
    output->axis[2][2] = input->axis[2][2];

    negativeTranslation[0] = vec3_origin[0] - input->origin[0];
    negativeTranslation[1] = vec3_origin[1] - input->origin[1];
    negativeTranslation[2] = vec3_origin[2] - input->origin[2];

    output->origin[0] = (float)(
        ((long double)negativeTranslation[1] * output->axis[1][0] +
         (long double)negativeTranslation[2] * output->axis[2][0]) +
        (long double)negativeTranslation[0] * output->axis[0][0]);
    output->origin[1] = (float)(
        ((long double)negativeTranslation[1] * output->axis[1][1] +
         (long double)negativeTranslation[2] * output->axis[2][1]) +
        (long double)negativeTranslation[0] * output->axis[0][1]);
    output->origin[2] = (float)(
        ((long double)negativeTranslation[2] * output->axis[2][2] +
         (long double)negativeTranslation[0] * output->axis[0][2]) +
        (long double)negativeTranslation[1] * output->axis[1][2]);
}

/*
 * MatrixTransformVector is present in every authoritative image.  The four
 * Windows bodies are instruction-identical:
 *
 *   CoDUOMP.exe                 0x00433320
 *   uo_cgame_mp_x86.dll        0x3004b480
 *   uo_ui_mp_x86.dll           0x40003450
 *   uo_game_mp_x86.dll         0x200184d0
 *
 * Windows evaluates output lane zero in Y,Z,X product order; lanes one and
 * two use X,Y,Z order.  Linux coduo_lnxded 0x08069154 and game module RVA
 * 0x0003cd97 are instruction-equivalent after ABI setup and use X,Y,Z for all
 * lanes.  Complete platform bodies preserve that genuine operation-graph
 * difference.  Each dot remains unspilled until its final binary32 store.
 */
void MatrixTransformVector(const vec3_t input, const axis_t matrix,
                           vec3_t output)
{
    output[0] = (float)(
        ((long double)input[1] * matrix[1][0] +
         (long double)input[2] * matrix[2][0]) +
        (long double)input[0] * matrix[0][0]);
    output[1] = (float)(
        ((long double)input[0] * matrix[0][1] +
         (long double)input[1] * matrix[1][1]) +
        (long double)input[2] * matrix[2][1]);
    output[2] = (float)(
        ((long double)input[0] * matrix[0][2] +
         (long double)input[1] * matrix[1][2]) +
        (long double)input[2] * matrix[2][2]);
}
