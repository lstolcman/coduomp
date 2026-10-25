#include "collision_brush_traces.h"


extern dshader_t *cm_materials;


/*
 * Complete swept-volume brush clipping primitive:
 *
 *   CoDUOMP.exe  0x00426a80..0x0042721b
 *   coduo_lnxded 0x08058f0a..0x08059874
 *
 * Both bodies clip the same enter/leave interval and select the same side.
 * Windows retains selected end distances and quotient comparisons at PC=53;
 * Linux stores those intermediates as binary32 under PC=64.  The separate
 * complete bodies preserve the original spill boundaries.
 */

void CM_TraceThroughBrush(
    traceWork_t *traceWork,
    const collisionBrush_t *brush)
{
    const float enterEpsilon = 0.125f;
    float enterFraction = 0.0f;
    float leaveFraction =
        traceWork->trace.fraction;
    qboolean allSolid = qtrue;
    const collisionBrushSide_t *hitSide = NULL;
    cplane_t axialPlane = {0};
    collisionBrushSide_t axialSide = {
        &axialPlane, 0
    };

    for (int32_t boundsPass = 0;
         boundsPass < 2;
         ++boundsPass) {
        const float sign =
            boundsPass == 0 ? -1.0f : 1.0f;
        const float *const brushBounds =
            boundsPass == 0
                ? brush->mins
                : brush->maxs;

        for (int32_t axis = 0; axis < 3; ++axis) {
            const float workOffset =
                traceWork->sphere.use != qfalse
                    ? traceWork->sphereExtents[axis]
                    : traceWork->maxs[axis];
            const float startDistance =
                (traceWork->start[axis] -
                 brushBounds[axis]) *
                    sign -
                workOffset;
            const long double endDistanceRaw =
                ((long double)traceWork->end[axis] -
                 brushBounds[axis]) *
                    sign -
                workOffset;

            if (startDistance > 0.0f) {
                const float fractionDelta = (float)(
                    (long double)startDistance -
                    endDistanceRaw);

                if (endDistanceRaw > 0.0L) {
                    if (fractionDelta <= 0.0f ||
                        endDistanceRaw >=
                            (long double)enterEpsilon) {
                        return;
                    }
                    allSolid = qfalse;
                }

                const long double enterLimitRaw =
                    (long double)startDistance -
                    enterEpsilon;
                if ((long double)enterFraction *
                        fractionDelta <
                    enterLimitRaw) {
                    /* 0x00426b8d/0x00426de4 stores the quotient as the next
                     * enter fraction, but compares the retained x87 value. */
                    const long double enterCandidate =
                        enterLimitRaw /
                        fractionDelta;
                    enterFraction = (float)enterCandidate;
                    if ((long double)leaveFraction <=
                        enterCandidate) {
                        return;
                    }
                } else if (hitSide != NULL) {
                    continue;
                }

                axialPlane.normal[0] = 0.0f;
                axialPlane.normal[1] = 0.0f;
                axialPlane.normal[2] = 0.0f;
                axialPlane.normal[axis] = sign;
                axialSide.materialIndex =
                    brush->axialMaterialIndices[
                        boundsPass * 3 + axis];
                hitSide = &axialSide;
            } else if (endDistanceRaw > 0.0L) {
                const long double fractionDeltaRaw =
                    (long double)startDistance -
                    endDistanceRaw;

                allSolid = qfalse;
                if ((long double)leaveFraction *
                        fractionDeltaRaw <
                    startDistance) {
                    leaveFraction = (float)(
                        (long double)startDistance /
                        fractionDeltaRaw);
                    if (leaveFraction <=
                        enterFraction) {
                        return;
                    }
                }
            }
        }
    }

    const collisionBrushSide_t *side =
        brush->nonAxialSides;
    for (int32_t sideCount = brush->nonAxialSideCount;
         sideCount != 0;
         --sideCount, ++side) {
        const cplane_t *const plane = side->plane;
        float startDistance;
        long double endDistanceRaw;

        if (traceWork->sphere.use != qfalse) {
            const long double planeDistanceRaw =
                (long double)plane->dist +
                traceWork->sphere.radius;
            const long double sphereOffsetRaw =
                ((long double)plane->normal[2] *
                      traceWork->sphere.offset[2] +
                  (long double)plane->normal[0] *
                      traceWork->sphere.offset[0]) +
                (long double)plane->normal[1] *
                    traceWork->sphere.offset[1];
            long double start[3];
            vec3_t end;

            if (sphereOffsetRaw > 0.0L) {
                for (int32_t axis = 0;
                     axis < 3;
                     ++axis) {
                    start[axis] =
                        (long double)traceWork->start[axis] -
                        traceWork->sphere.offset[axis];
                    end[axis] =
                        traceWork->end[axis] -
                        traceWork->sphere.offset[axis];
                }
            } else {
                for (int32_t axis = 0;
                     axis < 3;
                     ++axis) {
                    start[axis] =
                        (long double)traceWork->start[axis] +
                        traceWork->sphere.offset[axis];
                    end[axis] =
                        traceWork->end[axis] +
                        traceWork->sphere.offset[axis];
                }
            }

            startDistance = (float)(
                (start[2] * plane->normal[2] +
                 start[1] * plane->normal[1]) +
                start[0] * plane->normal[0] -
                planeDistanceRaw);
            endDistanceRaw =
                ((long double)end[2] *
                     plane->normal[2] +
                 (long double)end[1] *
                     plane->normal[1]) +
                (long double)end[0] *
                    plane->normal[0] -
                planeDistanceRaw;
        } else {
            const float *const offset =
                traceWork->offsets[plane->signbits];
            const long double planeDistanceRaw =
                (long double)plane->dist -
                (((long double)offset[2] *
                      plane->normal[2] +
                  (long double)offset[0] *
                      plane->normal[0]) +
                 (long double)offset[1] *
                     plane->normal[1]);
            startDistance = (float)(
                ((long double)traceWork->start[1] *
                     plane->normal[1] +
                 (long double)traceWork->start[0] *
                     plane->normal[0]) +
                (long double)traceWork->start[2] *
                    plane->normal[2] -
                planeDistanceRaw);
            endDistanceRaw =
                ((long double)traceWork->end[1] *
                     plane->normal[1] +
                 (long double)traceWork->end[2] *
                     plane->normal[2]) +
                (long double)traceWork->end[0] *
                    plane->normal[0] -
                planeDistanceRaw;
        }

        if (startDistance > 0.0f) {
            const float fractionDelta = (float)(
                (long double)startDistance -
                endDistanceRaw);

            if (endDistanceRaw > 0.0L) {
                if (fractionDelta <= 0.0f ||
                    endDistanceRaw >=
                        (long double)enterEpsilon) {
                    return;
                }
                allSolid = qfalse;
            }

            const long double enterLimitRaw =
                (long double)startDistance -
                enterEpsilon;
            if ((long double)enterFraction *
                    fractionDelta <
                enterLimitRaw) {
                /* 0x00426f17/0x004270e4 follows the same retained quotient
                 * rule for arbitrary brush sides. */
                const long double enterCandidate =
                    enterLimitRaw /
                    fractionDelta;
                enterFraction = (float)enterCandidate;
                if ((long double)leaveFraction <=
                    enterCandidate) {
                    return;
                }
            } else if (hitSide != NULL) {
                continue;
            }

            hitSide = side;
        } else if (endDistanceRaw > 0.0L) {
            const long double fractionDeltaRaw =
                (long double)startDistance -
                endDistanceRaw;

            allSolid = qfalse;
            if ((long double)leaveFraction *
                    fractionDeltaRaw <
                startDistance) {
                leaveFraction = (float)(
                    (long double)startDistance /
                    fractionDeltaRaw);
                if (leaveFraction <=
                    enterFraction) {
                    return;
                }
            }
        }
    }

    traceWork->trace.contents = brush->contents;
    if (hitSide == NULL) {
        traceWork->trace.startsolid = qtrue;
        if (allSolid != qfalse) {
            traceWork->trace.allsolid = qtrue;
            traceWork->trace.fraction = 0.0f;
        }
        return;
    }

    traceWork->trace.fraction = enterFraction;
    for (int32_t axis = 0; axis < 3; ++axis) {
        traceWork->trace.normal[axis] =
            hitSide->plane->normal[axis];
    }
    dshader_t *const material =
        &cm_materials[hitSide->materialIndex];
    traceWork->trace.surfaceFlags =
        material->surfaceFlags;
    traceWork->trace.material =
        material->shader;
}
