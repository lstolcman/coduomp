#include "collision_geometry.h"

#include "math/q_math.h"

#include <stdint.h>


/*
 * Complete collision-local rotation helper cluster:
 *
 *   RotatePoint          CoDUOMP.exe 0x00425d70, coduo_lnxded 0x08057b18
 *   TransposeMatrix      CoDUOMP.exe 0x00425dc0, coduo_lnxded 0x08057bbf
 *   CreateRotationMatrix CoDUOMP.exe 0x00425df0, coduo_lnxded 0x08057c2a
 *
 * CreateRotationMatrix and TransposeMatrix have common source behavior.
 * RotatePoint retains the genuine per-platform x87 graph below.
 */

void CreateRotationMatrix(const vec3_t angles, axis_t matrix)
{
    AngleVectors(angles, matrix[0], matrix[1], matrix[2]);
    VectorInverse(matrix[1]);
}

void RotatePoint(vec3_t point, const axis_t matrix)
{
    const long double x = (long double)point[0];
    const long double y = (long double)point[1];
    const long double z = (long double)point[2];

    for (int32_t row = 0; row < 3; ++row) {
        point[row] = (float)(
            (z * (long double)matrix[row][2] +
             y * (long double)matrix[row][1]) +
            x * (long double)matrix[row][0]);
    }
}

void TransposeMatrix(const axis_t input, axis_t output)
{
    for (int32_t row = 0; row < 3; ++row) {
        for (int32_t column = 0; column < 3; ++column) {
            output[row][column] = input[column][row];
        }
    }
}
