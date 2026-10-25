#include "q_math.h"


#include <stdint.h>


/*
 * Complete compact 4x3 inverse/in-place transform bank.  The retained
 * Windows functions are byte-identical in all four authoritative images:
 *
 *   CoDUOMP.exe                 0x00433460, 0x004334e0
 *   uo_cgame_mp_x86.dll        0x3004b5c0, 0x3004b640
 *   uo_ui_mp_x86.dll           0x40003590, 0x40003610
 *   uo_game_mp_x86.dll         0x20018610, 0x20018690
 *
 * The Linux game-module functions at RVAs 0x0003d059 and 0x0003d118 are
 * byte-identical to the engine functions at 0x08069416 and 0x080694d5.
 * Both platforms stage the inverse translation as binary32 and preserve
 * in-place input values, but their x87 multiply/add graphs differ.  Keep each
 * platform body whole so its accumulation order remains explicit.
 */


void MatrixTransposeTransformVector43(const vec3_t input,
                                      const matrix43_t *matrix,
                                      vec3_t output)
{
    float difference[3];

    difference[0] = (float)((long double)input[0] - matrix->origin[0]);
    difference[1] = (float)((long double)input[1] - matrix->origin[1]);
    difference[2] = (float)((long double)input[2] - matrix->origin[2]);

    for (int32_t row = 0; row < 3; ++row) {
        output[row] = (float)(
            ((long double)difference[2] * matrix->axis[row][2] +
             (long double)difference[1] * matrix->axis[row][1]) +
            (long double)difference[0] * matrix->axis[row][0]);
    }
}

void MatrixTransformVector43Equals(vec3_t vector,
                                   const matrix43_t *matrix)
{
    float output[3];

    output[0] = (float)(
        (((long double)matrix->axis[1][0] * vector[1] +
          (long double)matrix->axis[2][0] * vector[2]) +
         (long double)matrix->axis[0][0] * vector[0]) +
        matrix->origin[0]);
    output[1] = (float)(
        (((long double)matrix->axis[1][1] * vector[1] +
          (long double)matrix->axis[0][1] * vector[0]) +
         (long double)matrix->axis[2][1] * vector[2]) +
        matrix->origin[1]);
    output[2] = (float)(
        (((long double)matrix->axis[1][2] * vector[1] +
          (long double)matrix->axis[0][2] * vector[0]) +
         (long double)matrix->axis[2][2] * vector[2]) +
        matrix->origin[2]);

    vector[0] = output[0];
    vector[1] = output[1];
    vector[2] = output[2];
}
