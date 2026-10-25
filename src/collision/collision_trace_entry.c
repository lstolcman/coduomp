#include "collision_trace_entry.h"

#include "collision_brush_traces.h"
#include "collision_capsule_traces.h"
#include "collision_leaf_traces.h"
#include "collision_patch_dispatch.h"
#include "collision_patch_trace.h"
#include "collision_queries.h"
#include "collision_terrain_dispatch.h"
#include "collision_trace_bounds.h"
#include "collision_tree_traces.h"
#include "math/q_math.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

extern collisionBrush_t *cm_boxBrush;
extern collisionBrush_t *cm_brushes;
extern collisionTerrainPatch_t *cm_terrainPatches;
extern int32_t cm_checkcount;
extern int32_t cm_numBrushes;
extern int32_t cm_numTerrainPatches;

void *Com_Memset(void *destination, int value, size_t count);
float CM_TraceFloatAbs(float value);
/*
 * Complete collision trace entrypoint pair:
 *
 *   CoDUOMP.exe  CM_Trace      0x00427ed0..0x00428569
 *                CM_SightTrace 0x00429b20..0x0042a175
 *   coduo_lnxded CM_Trace      0x0805a9d5..0x0805b144
 *                CM_SightTrace 0x0805cd62..0x0805d458
 *
 * The same dispatch and result contracts are preserved. Platform bodies keep
 * their proven x87 expression graphs, spills, and cached-hit realization.
 */
/* Source: CoDUOMP.exe 0x00427ed0..0x00428569.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00427ed0_0042856a.mcode.
 * Name: exact same-module Mac symbol CM_Trace. Build the centered moving
 * trace record, derive its corner offsets and swept bounds, dispatch either
 * the stationary or moving collision path, and copy the final trace out. */
void CM_Trace(trace_t *trace,
              const vec3_t start, const vec3_t end,
              const vec3_t mins, const vec3_t maxs,
              int32_t modelHandle, int32_t contentMask,
              qboolean capsule,
              const cmTraceSphereRecord_t *sphere)
{
    const collisionModel_t *const collisionModel =
        CM_ClipHandleToModel(modelHandle);
    traceWork_t traceWork;
    long double deltaZRaw = 0.0L;

    cm_checkcount++;
    Com_Memset(&traceWork, 0, sizeof(traceWork));
    traceWork.trace.fraction = trace->fraction;

    const float *const traceMins =
        mins != NULL ? mins : vec3_origin;
    const float *const traceMaxs =
        maxs != NULL ? maxs : vec3_origin;

    for (int32_t axis = 0; axis < 3; ++axis) {
        /* 0x427f3f..0x427fed: the centered offset and end remain in
         * x87 precision while their float copies are written.  The delta
         * subtracts the rounded centered start from the retained end. */
        const long double offsetRaw =
            ((long double)traceMins[axis] +
             (long double)traceMaxs[axis]) *
            0.5L;
        traceWork.mins[axis] = (float)(
            (long double)traceMins[axis] - offsetRaw);
        traceWork.maxs[axis] = (float)(
            (long double)traceMaxs[axis] - offsetRaw);
        traceWork.start[axis] = (float)(
            (long double)start[axis] + offsetRaw);

        const long double endRaw =
            (long double)end[axis] + offsetRaw;
        traceWork.end[axis] = (float)endRaw;
        const long double deltaRaw =
            endRaw - (long double)traceWork.start[axis];
        traceWork.delta[axis] = (float)deltaRaw;
        if (axis == 2)
            deltaZRaw = deltaRaw;
    }
    /* 0x427fed..0x428009: only the Z delta remains live after its float
     * store; the Y and X terms are reloaded from their rounded copies. */
    traceWork.deltaLengthSquared = (float)(
        deltaZRaw * (long double)traceWork.delta[2] +
        (long double)traceWork.delta[1] *
            (long double)traceWork.delta[1] +
        (long double)traceWork.delta[0] *
            (long double)traceWork.delta[0]);

    if (sphere != NULL) {
        /* The original copies this complete 36-byte record as nine dwords.
         * A raw copy also permits transformed traces to leave the extents
         * unspecified: moving sphere traces replace them before use. */
        memcpy(&traceWork.sphere, &sphere->sphere,
               sizeof(traceWork.sphere));
        memcpy(traceWork.sphereExtents,
               sphere->extents,
               sizeof(traceWork.sphereExtents));
    } else {
        traceWork.sphere.use = capsule;
        traceWork.sphere.radius =
            traceWork.maxs[2] <
                    traceWork.maxs[0]
                ? traceWork.maxs[2]
                : traceWork.maxs[0];
        traceWork.sphere.halfheight =
            traceWork.maxs[2];
        traceWork.sphere.offset[0] = 0.0f;
        traceWork.sphere.offset[1] = 0.0f;
        traceWork.sphere.offset[2] =
            traceWork.maxs[2] -
            traceWork.sphere.radius;
    }

    /* 0x428087..0x4280af leaves this raw sum on the x87 stack until the
     * moving-trace point test at 0x4283fd..0x428417. */
    const long double maxsSumRaw =
        ((long double)traceWork.maxs[1] +
         (long double)traceWork.maxs[2]) +
        (long double)traceWork.maxs[0];
    traceWork.maxsSum = (float)maxsSumRaw;
    traceWork.contents = contentMask;

    traceWork.offsets[0][0] =
        traceWork.mins[0];
    traceWork.offsets[0][1] =
        traceWork.mins[1];
    traceWork.offsets[0][2] =
        traceWork.mins[2];
    traceWork.offsets[1][0] =
        traceWork.maxs[0];
    traceWork.offsets[1][1] =
        traceWork.mins[1];
    traceWork.offsets[1][2] =
        traceWork.mins[2];
    traceWork.offsets[2][0] =
        traceWork.mins[0];
    traceWork.offsets[2][1] =
        traceWork.maxs[1];
    traceWork.offsets[2][2] =
        traceWork.mins[2];
    traceWork.offsets[3][0] =
        traceWork.maxs[0];
    traceWork.offsets[3][1] =
        traceWork.maxs[1];
    traceWork.offsets[3][2] =
        traceWork.mins[2];
    traceWork.offsets[4][0] =
        traceWork.mins[0];
    traceWork.offsets[4][1] =
        traceWork.mins[1];
    traceWork.offsets[4][2] =
        traceWork.maxs[2];
    traceWork.offsets[5][0] =
        traceWork.maxs[0];
    traceWork.offsets[5][1] =
        traceWork.mins[1];
    traceWork.offsets[5][2] =
        traceWork.maxs[2];
    traceWork.offsets[6][0] =
        traceWork.mins[0];
    traceWork.offsets[6][1] =
        traceWork.maxs[1];
    traceWork.offsets[6][2] =
        traceWork.maxs[2];
    traceWork.offsets[7][0] =
        traceWork.maxs[0];
    traceWork.offsets[7][1] =
        traceWork.maxs[1];
    traceWork.offsets[7][2] =
        traceWork.maxs[2];

    if (traceWork.sphere.use != qfalse) {
        for (int32_t axis = 0; axis < 3; ++axis) {
            const float sphereOffset =
                fabsf(traceWork.sphere.offset[axis]);
            if (traceWork.start[axis] <
                traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] -
                    sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] +
                    sphereOffset +
                    traceWork.sphere.radius;
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] -
                    sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] +
                    sphereOffset +
                    traceWork.sphere.radius;
            }
        }
    } else {
        for (int32_t axis = 0; axis < 3; ++axis) {
            if (traceWork.start[axis] <
                traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] +
                    traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] +
                    traceWork.maxs[axis];
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] +
                    traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] +
                    traceWork.maxs[axis];
            }
        }
    }

    if (start[0] == end[0] &&
        start[1] == end[1] &&
        start[2] == end[2]) {
        if (modelHandle == CM_WORLD_MODEL) {
            CM_PositionTest(&traceWork);
        } else if (
            modelHandle ==
            CM_TEMP_CAPSULE_MODEL_HANDLE) {
            if ((cm_boxBrush->contents &
                 contentMask) != 0) {
                if (traceWork.sphere.use !=
                    qfalse) {
                    CM_TestCapsuleInCapsule(
                        &traceWork);
                } else {
                    CM_TestBoundingBoxInCapsule(
                        &traceWork);
                }
            }
        } else {
            CM_TestInLeaf(
                &traceWork,
                &collisionModel->leaf);
        }
    } else {
        traceWork.isPoint = maxsSumRaw == 0.0L;

        if (traceWork.sphere.use != qfalse) {
            for (int32_t axis = 0;
                 axis < 3; ++axis) {
                traceWork.sphereExtents[axis] =
                    traceWork.sphere.radius +
                    fabsf(
                        traceWork.sphere
                            .offset[axis]);
            }
        }

        if (modelHandle == CM_WORLD_MODEL) {
            CM_TraceThroughTree(
                &traceWork, 0, 0.0f,
                traceWork.trace.fraction,
                traceWork.start, traceWork.end);
        } else if (
            modelHandle ==
            CM_TEMP_CAPSULE_MODEL_HANDLE) {
            if ((cm_boxBrush->contents &
                 traceWork.contents) != 0) {
                if (traceWork.sphere.use !=
                    qfalse) {
                    CM_TraceCapsuleThroughCapsule(
                        &traceWork);
                } else {
                    CM_TraceBoundingBoxThroughCapsule(
                        &traceWork);
                }
            }
        } else {
            CM_TraceThroughLeaf(
                &traceWork,
                &collisionModel->leaf);
        }
    }

    for (int32_t axis = 0; axis < 3; ++axis) {
        traceWork.trace.endpos[axis] =
            start[axis] +
            traceWork.delta[axis] *
                traceWork.trace.fraction;
    }
    *trace = traceWork.trace;
}

/* Source: CoDUOMP.exe 0x00429b20..0x0042a175.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00429b20_0042a176.mcode.
 * Name: exact same-module Mac symbol CM_SightTrace. Build the centered
 * sight-work record, optionally recheck a prior encoded hit, and dispatch
 * world, inline-model, or temporary-capsule collision. */
int32_t CM_SightTrace(
    int32_t oldHitNum,
    const vec3_t start, const vec3_t end,
    const vec3_t mins, const vec3_t maxs,
    int32_t modelHandle, const vec3_t origin,
    int32_t contentMask, qboolean capsule,
    const cmTraceSphereRecord_t *sphere)
{
    (void)origin;

    const collisionModel_t *const
        collisionModel =
            CM_ClipHandleToModel(modelHandle);
    traceWork_t traceWork;
    long double deltaZRaw = 0.0L;

    cm_checkcount++;
    Com_Memset(
        &traceWork, 0, sizeof(traceWork));
    traceWork.trace.fraction = 1.0f;

    const float *const traceMins =
        mins != NULL ? mins : vec3_origin;
    const float *const traceMaxs =
        maxs != NULL ? maxs : vec3_origin;
    traceWork.contents = contentMask;

    for (int32_t axis = 0; axis < 3;
         ++axis) {
        /* 0x429b81..0x429c38 mirrors CM_Trace: the centering offset and
         * centered end remain wide while their float copies are stored. */
        const long double offsetRaw =
            ((long double)traceMins[axis] +
             (long double)traceMaxs[axis]) *
            0.5L;
        traceWork.mins[axis] = (float)(
            (long double)traceMins[axis] - offsetRaw);
        traceWork.maxs[axis] = (float)(
            (long double)traceMaxs[axis] - offsetRaw);
        traceWork.start[axis] = (float)(
            (long double)start[axis] + offsetRaw);
        const long double endRaw =
            (long double)end[axis] + offsetRaw;
        traceWork.end[axis] = (float)endRaw;
        const long double deltaRaw =
            endRaw - (long double)traceWork.start[axis];
        traceWork.delta[axis] = (float)deltaRaw;
        if (axis == 2)
            deltaZRaw = deltaRaw;
    }
    traceWork.deltaLengthSquared = (float)(
        deltaZRaw * (long double)traceWork.delta[2] +
        (long double)traceWork.delta[1] *
            (long double)traceWork.delta[1] +
        (long double)traceWork.delta[0] *
            (long double)traceWork.delta[0]);

    if (sphere != NULL) {
        memcpy(
            &traceWork.sphere,
            &sphere->sphere,
            sizeof(traceWork.sphere));
        memcpy(
            traceWork.sphereExtents,
            sphere->extents,
            sizeof(traceWork.sphereExtents));
    } else {
        traceWork.sphere.use = capsule;
        traceWork.sphere.radius =
            traceWork.maxs[2] <
                    traceWork.maxs[0]
                ? traceWork.maxs[2]
                : traceWork.maxs[0];
        traceWork.sphere.halfheight =
            traceWork.maxs[2];
        traceWork.sphere.offset[0] = 0.0f;
        traceWork.sphere.offset[1] = 0.0f;
        traceWork.sphere.offset[2] =
            traceWork.maxs[2] -
            traceWork.sphere.radius;
    }

    const long double maxsSumRaw =
        ((long double)traceWork.maxs[1] +
         (long double)traceWork.maxs[2]) +
        (long double)traceWork.maxs[0];
    traceWork.maxsSum = (float)maxsSumRaw;

    traceWork.offsets[0][0] =
        traceWork.mins[0];
    traceWork.offsets[0][1] =
        traceWork.mins[1];
    traceWork.offsets[0][2] =
        traceWork.mins[2];
    traceWork.offsets[1][0] =
        traceWork.maxs[0];
    traceWork.offsets[1][1] =
        traceWork.mins[1];
    traceWork.offsets[1][2] =
        traceWork.mins[2];
    traceWork.offsets[2][0] =
        traceWork.mins[0];
    traceWork.offsets[2][1] =
        traceWork.maxs[1];
    traceWork.offsets[2][2] =
        traceWork.mins[2];
    traceWork.offsets[3][0] =
        traceWork.maxs[0];
    traceWork.offsets[3][1] =
        traceWork.maxs[1];
    traceWork.offsets[3][2] =
        traceWork.mins[2];
    traceWork.offsets[4][0] =
        traceWork.mins[0];
    traceWork.offsets[4][1] =
        traceWork.mins[1];
    traceWork.offsets[4][2] =
        traceWork.maxs[2];
    traceWork.offsets[5][0] =
        traceWork.maxs[0];
    traceWork.offsets[5][1] =
        traceWork.mins[1];
    traceWork.offsets[5][2] =
        traceWork.maxs[2];
    traceWork.offsets[6][0] =
        traceWork.mins[0];
    traceWork.offsets[6][1] =
        traceWork.maxs[1];
    traceWork.offsets[6][2] =
        traceWork.maxs[2];
    traceWork.offsets[7][0] =
        traceWork.maxs[0];
    traceWork.offsets[7][1] =
        traceWork.maxs[1];
    traceWork.offsets[7][2] =
        traceWork.maxs[2];

    if (traceWork.sphere.use != qfalse) {
        for (int32_t axis = 0; axis < 3;
             ++axis) {
            const float sphereOffset =
                fabsf(
                    traceWork.sphere
                        .offset[axis]);
            if (traceWork.start[axis] <
                traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] -
                    sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] +
                    sphereOffset +
                    traceWork.sphere.radius;
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] -
                    sphereOffset -
                    traceWork.sphere.radius;
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] +
                    sphereOffset +
                    traceWork.sphere.radius;
            }
        }
    } else {
        for (int32_t axis = 0; axis < 3;
             ++axis) {
            if (traceWork.start[axis] <
                traceWork.end[axis]) {
                traceWork.bounds[0][axis] =
                    traceWork.start[axis] +
                    traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.end[axis] +
                    traceWork.maxs[axis];
            } else {
                traceWork.bounds[0][axis] =
                    traceWork.end[axis] +
                    traceWork.mins[axis];
                traceWork.bounds[1][axis] =
                    traceWork.start[axis] +
                    traceWork.maxs[axis];
            }
        }
    }

    traceWork.isPoint = maxsSumRaw == 0.0L;
    if (traceWork.sphere.use != qfalse) {
        for (int32_t axis = 0; axis < 3;
             ++axis) {
            traceWork.sphereExtents[axis] =
                traceWork.sphere.radius +
                fabsf(
                    traceWork.sphere
                        .offset[axis]);
        }
    }

    if (modelHandle != CM_WORLD_MODEL) {
        if (modelHandle ==
            CM_TEMP_CAPSULE_MODEL_HANDLE) {
            if ((cm_boxBrush->contents &
                 traceWork.contents) == 0) {
                return 0;
            }
            if (traceWork.sphere.use !=
                qfalse) {
                return
                    CM_SightTraceCapsuleThroughCapsule(
                        &traceWork);
            }
            return
                CM_SightTraceBoundingBoxThroughCapsule(
                    &traceWork);
        }

        return CM_SightTraceThroughLeaf(
            &traceWork,
            &collisionModel->leaf);
    }

    int32_t sightHit = 0;
    if (oldHitNum > 0) {
        const int32_t hitIndex =
            oldHitNum - 1;
        if (hitIndex < cm_numBrushes) {
            sightHit =
                CM_SightTraceThroughBrush(
                    &traceWork,
                    &cm_brushes[hitIndex]);
        } else {
            const int32_t terrainPatchIndex =
                hitIndex - cm_numBrushes;
            if (terrainPatchIndex <
                cm_numTerrainPatches) {
                collisionTerrainPatch_t *const
                    terrainPatch =
                        &cm_terrainPatches[
                            terrainPatchIndex];
                /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
                if (CM_TraceWorkIntersectsBounds(
                        &traceWork,
                        terrainPatch->bounds[0],
                        terrainPatch->bounds[1]) ==
                    qfalse) {
                    return qtrue;
                }

                if (terrainPatch->curveCollide !=
                    NULL) {
                    sightHit =
                        CM_SightTraceThroughPatchCollide(
                            &traceWork,
                            terrainPatch
                                ->curveCollide);
                } else {
                    sightHit =
                        CM_SightTraceThroughTerrainCollide(
                            &traceWork,
                            terrainPatch
                                ->terrainCollide);
                }
            }
        }
    }

    if (sightHit != 0)
        return sightHit;

    return CM_SightTraceThroughTree(
        &traceWork, 0, 0.0f, 1.0f,
        traceWork.start, traceWork.end);
}
