#include "collision_trace_bounds.h"


#include <stdint.h>


/*
 * Complete collision broad-phase bounds-rejection cluster:
 *
 *   CoDUOMP.exe  CM_TraceWorkIntersectsBounds 0x00426820..0x004269ea
 *                CM_TraceLineSkipsBox         0x0042ad30..0x0042ae33
 *   coduo_lnxded CM_TraceWorkIntersectsBounds 0x08058b85..0x08058e2e
 *                CM_TraceLineSkipsBox         0x0805e387..0x0805e4fc
 *
 * The predicates and stored results agree.  The original Windows compiler
 * retains selected separating-axis and interval values in x87 registers,
 * while the Linux compiler spills those values to binary32.  Whole-function
 * behavior bodies preserve those proven realization boundaries without
 * splitting ownership of the subsystem.
 */

qboolean CM_TraceWorkIntersectsBounds(const traceWork_t *traceWork,
                                      const vec3_t mins,
                                      const vec3_t maxs)
{
    for (int32_t axis = 0; axis < 3; ++axis) {
        if (maxs[axis] < traceWork->bounds[0][axis] ||
            traceWork->bounds[1][axis] < mins[axis]) {
            return qfalse;
        }
    }

    if (traceWork->isPoint == qfalse) {
        return qtrue;
    }

    vec3_t boundsSize;
    vec3_t traceBoundsSize;
    for (int32_t axis = 0; axis < 3; ++axis) {
        boundsSize[axis] = maxs[axis] - mins[axis];
        traceBoundsSize[axis] =
            traceWork->bounds[1][axis] - traceWork->bounds[0][axis];
    }

    /* X subtracts two live sums; Y reloads only the trace sum. Z reloads
     * both sums but retains their difference through the cross products. */
    const float centerDeltaX = (float)(
        ((long double)traceWork->end[0] + traceWork->start[0]) -
        ((long double)maxs[0] + mins[0]));
    const float traceCenterY = (float)((long double)traceWork->end[1] + traceWork->start[1]);
    const float centerDeltaY = (float)((long double)traceCenterY - ((long double)maxs[1] + mins[1]));
    const float boundsCenterZ = (float)((long double)maxs[2] + mins[2]);
    const float traceCenterZ = (float)((long double)traceWork->end[2] + traceWork->start[2]);
    const long double centerDeltaZRaw = (long double)traceCenterZ - boundsCenterZ;
    const long double crossX =
        (long double)traceWork->delta[1] * centerDeltaZRaw -
        (long double)traceWork->delta[2] * centerDeltaY;
    const long double projectedX =
        (long double)boundsSize[1] * traceBoundsSize[2] +
        (long double)boundsSize[2] * traceBoundsSize[1];
    if (projectedX * projectedX < crossX * crossX) {
        return qfalse;
    }

    const long double crossY =
        (long double)traceWork->delta[2] * centerDeltaX -
        (long double)traceWork->delta[0] * centerDeltaZRaw;
    const long double projectedY =
        (long double)boundsSize[0] * traceBoundsSize[2] +
        (long double)boundsSize[2] * traceBoundsSize[0];
    if (projectedY * projectedY < crossY * crossY) {
        return qfalse;
    }

    const long double crossZ =
        (long double)traceWork->delta[0] * centerDeltaY -
        (long double)traceWork->delta[1] * centerDeltaX;
    const long double projectedZ =
        (long double)boundsSize[0] * traceBoundsSize[1] +
        (long double)boundsSize[1] * traceBoundsSize[0];
    if (projectedZ * projectedZ < crossZ * crossZ) {
        return qfalse;
    }

    return qtrue;
}

qboolean CM_TraceLineSkipsBox(const vec3_t start, const vec3_t end,
                              const vec3_t mins, const vec3_t maxs,
                              float fraction)
{
    float enterFraction = 0.0f;
    float leaveFraction = fraction;
    float sideScale = -1.0f;
    const float *bounds = mins;

    for (;;) {
        for (int32_t axis = 0; axis < 3; ++axis) {
            const float startOffset = (float)(
                ((long double)start[axis] - bounds[axis]) * sideScale);
            const long double endOffset =
                ((long double)end[axis] - bounds[axis]) * sideScale;

            if (startOffset > 0.0f) {
                if (endOffset > 0.0L) {
                    return qtrue;
                }
                const long double offsetDelta =
                    (long double)startOffset - endOffset;
                if ((long double)enterFraction * offsetDelta < startOffset) {
                    enterFraction = (float)(
                        (long double)startOffset / offsetDelta);
                    if (leaveFraction <= enterFraction) {
                        return qtrue;
                    }
                }
            } else if (endOffset > 0.0L) {
                const long double offsetDelta =
                    (long double)startOffset - endOffset;
                if ((long double)leaveFraction * offsetDelta < startOffset) {
                    leaveFraction = (float)(
                        (long double)startOffset / offsetDelta);
                    if (leaveFraction <= enterFraction) {
                        return qtrue;
                    }
                }
            }
        }

        if (sideScale == 1.0f) {
            return qfalse;
        }
        sideScale = 1.0f;
        bounds = maxs;
    }
}
