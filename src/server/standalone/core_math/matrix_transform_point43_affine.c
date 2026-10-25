
#include "core_math_private.h"

void MatrixTransformPoint43Affine(const vec3_t in, const DObjSkelMat *matrix,
                  vec3_t out)
{
    out[0] = (((in[0] * matrix->axis[0][0]) +
               (in[1] * matrix->axis[1][0])) +
              (in[2] * matrix->axis[2][0])) +
             matrix->origin[0];
    out[1] = (((in[0] * matrix->axis[0][1]) +
               (in[1] * matrix->axis[1][1])) +
              (in[2] * matrix->axis[2][1])) +
             matrix->origin[1];
    out[2] = (((in[0] * matrix->axis[0][2]) +
               (in[1] * matrix->axis[1][2])) +
              (in[2] * matrix->axis[2][2])) +
             matrix->origin[2];
}
