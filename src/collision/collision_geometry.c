#include "collision_geometry.h"


#include <math.h>
#include <stdint.h>
#include <string.h>


enum {
    CM_VECTOR_AXIS_COUNT = 3
};

#define CM_FAST_INV_SQRT_MAGIC UINT32_C(0x5f3759df)
#define CM_FLOAT_SIGN_BIT UINT32_C(0x80000000)

/*
 * Complete collision projection/distance cluster:
 *
 *   CoDUOMP.exe   0x00425e20..0x00425ff8
 *   coduo_lnxded  0x08057c66..0x08057fe2
 *
 * The signatures and geometric decisions agree.  The whole-function bodies
 * below preserve genuine compiler/platform differences in x87 operation
 * order and binary32 spill points.
 */

/* Windows retains all three point-line deltas in x87 registers and recomputes
 * the dot product as (Z+Y)+X for each output lane. */
void CM_ProjectPointOntoLine(const vec3_t point, const vec3_t linePoint,
                             const vec3_t lineDirection, vec3_t projected)
{
    const long double deltaX =
        (long double)point[0] - (long double)linePoint[0];
    const long double deltaY =
        (long double)point[1] - (long double)linePoint[1];
    const long double deltaZ =
        (long double)point[2] - (long double)linePoint[2];

    for (int32_t axis = 0; axis < CM_VECTOR_AXIS_COUNT; ++axis) {
        const long double dot =
            (deltaZ * (long double)lineDirection[2] +
             deltaY * (long double)lineDirection[1]) +
            deltaX * (long double)lineDirection[0];
        projected[axis] = (float)(
            dot * (long double)lineDirection[axis] +
            (long double)linePoint[axis]);
    }
}

/* The endpoint scan and strict nearest-endpoint choice agree.  Windows keeps
 * the selected point deltas and (Z+Y)+X squared sum live through return. */
long double CM_DistanceSquaredPointToSegment(const vec3_t point,
                                             const vec3_t start,
                                             const vec3_t end,
                                             const vec3_t direction)
{
    vec3_t projected;
    CM_ProjectPointOntoLine(point, start, direction, projected);

    int32_t axis;
    for (axis = 0;
         axis < CM_VECTOR_AXIS_COUNT &&
         !(projected[axis] > start[axis] &&
           projected[axis] > end[axis]) &&
         !(start[axis] > projected[axis] &&
           end[axis] > projected[axis]);
         ++axis) {
    }

    const float *endpoint;
    if (axis < CM_VECTOR_AXIS_COUNT) {
        endpoint =
            fabsl((long double)projected[axis] -
                  (long double)start[axis]) <
                    fabsl((long double)projected[axis] -
                          (long double)end[axis])
                ? start
                : end;
    } else {
        endpoint = projected;
    }

    const long double deltaX =
        (long double)point[0] - (long double)endpoint[0];
    const long double deltaY =
        (long double)point[1] - (long double)endpoint[1];
    const long double deltaZ =
        (long double)point[2] - (long double)endpoint[2];
    return (deltaZ * deltaZ + deltaY * deltaY) + deltaX * deltaX;
}

long double CM_DistanceSquared(const vec3_t start, const vec3_t end)
{
    const long double deltaX =
        (long double)end[0] - (long double)start[0];
    const long double deltaY =
        (long double)end[1] - (long double)start[1];
    const long double deltaZ =
        (long double)end[2] - (long double)start[2];
    return (deltaZ * deltaZ + deltaY * deltaY) + deltaX * deltaX;
}

/* Both binaries use the signed shift form of the 0x5f3759df seed.  Windows
 * keeps value*0.5 live, stores only the first Newton result as binary32, and
 * carries the second refinement directly into the final multiply.  Linux
 * spills the half-value and both Newton results as binary32. */
long double CM_FastSqrt(float value)
{
    uint32_t bits;
    float inverseRoot;
    memcpy(&bits, &value, sizeof(bits));
    bits = CM_FAST_INV_SQRT_MAGIC -
           ((bits >> 1) | (bits & CM_FLOAT_SIGN_BIT));
    memcpy(&inverseRoot, &bits, sizeof(inverseRoot));

    const long double half = (long double)value * 0.5L;
    inverseRoot = (float)(
        (long double)inverseRoot *
        (1.5L - half * (long double)inverseRoot *
                    (long double)inverseRoot));
    const long double refined =
        (long double)inverseRoot *
        (1.5L - half * (long double)inverseRoot *
                    (long double)inverseRoot);
    return (long double)value * refined;
}

#undef CM_FLOAT_SIGN_BIT
#undef CM_FAST_INV_SQRT_MAGIC
