
#include "core_math_private.h"

#include <string.h>


void MatrixTransformPoint43CompactInPlace(vec3_t point,
                  const matrix43_t *matrix)
{
    const float point0 = point[0];
    const float point1 = point[1];
    const float point2 = point[2];

    const float transformed0 =
        (((point0 * matrix->axis[0][0]) + (point1 * matrix->axis[1][0])) +
         (point2 * matrix->axis[2][0])) +
        matrix->origin[0];
    const float transformed1 =
        (((point0 * matrix->axis[0][1]) + (point1 * matrix->axis[1][1])) +
         (point2 * matrix->axis[2][1])) +
        matrix->origin[1];

    point[2] = (((point0 * matrix->axis[0][2]) +
                 (point1 * matrix->axis[1][2])) +
                (point2 * matrix->axis[2][2])) +
               matrix->origin[2];
    memcpy(&point[0], &transformed0, sizeof(point[0]));
    memcpy(&point[1], &transformed1, sizeof(point[1]));
}
