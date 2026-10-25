#include "collision_box_trace.h"

#include "collision_geometry.h"
#include "collision_queries.h"
#include "collision_trace_entry.h"
#include "qcommon/collision_trace_work_types.h"

#include <stddef.h>
#include <stdint.h>


/*
 * Complete public box-trace entry cluster:
 *
 *   CoDUOMP.exe  CM_BoxTrace                    0x00428570
 *                CM_TransformedBoxTrace         0x004285a0
 *                CM_TransformedBoxTraceExternal 0x00428a60
 *                CM_BoxSightTrace               0x0042a180
 *                CM_TransformedBoxSightTrace    0x0042a1b0
 *   coduo_lnxded CM_BoxTrace                    0x0805b145
 *                CM_TransformedBoxTrace         0x0805b19a
 *                CM_TransformedBoxTraceExternal 0x0805b5b0
 *                CM_BoxSightTrace               0x0805d459
 *                CM_TransformedBoxSightTrace    0x0805d4ad
 *
 * Both targets recenter and store every input lane as binary32, transform the
 * same sphere/capsule record, invoke CM_Trace, and reconstruct the world-space
 * endpoint with the same unspilled multiply/add chain.  Their numeric behavior
 * agrees.  Narrow behavior gates below retain only the original Windows FCOMP
 * versus Linux FUCOMPP distinction for x87 exception state.
 */

void CM_BoxTrace(trace_t *trace, const vec3_t start, const vec3_t end,
                 const vec3_t mins, const vec3_t maxs,
                 int32_t modelHandle, int32_t contentMask,
                 qboolean capsule)
{
    trace->fraction = 1.0f;
    CM_Trace(trace, start, end, mins, maxs,
             modelHandle, contentMask, capsule, NULL);
}

void CM_TransformedBoxTrace(trace_t *trace,
                            const vec3_t start, const vec3_t end,
                            const vec3_t mins, const vec3_t maxs,
                            int32_t modelHandle, int32_t contentMask,
                            const vec3_t origin, const vec3_t angles,
                            qboolean capsule)
{
    static const vec3_t zero = {0.0f, 0.0f, 0.0f};
    const float *const inputMins = mins != NULL ? mins : zero;
    const float *const inputMaxs = maxs != NULL ? maxs : zero;
    vec3_t traceMins;
    vec3_t traceMaxs;
    vec3_t startLocal;
    vec3_t endLocal;

    for (int32_t axis = 0; axis < 3; ++axis) {
        const float offset = (float)(
            ((long double)inputMins[axis] +
             (long double)inputMaxs[axis]) * 0.5L);
        traceMins[axis] = (float)(
            (long double)inputMins[axis] - (long double)offset);
        traceMaxs[axis] = (float)(
            (long double)inputMaxs[axis] - (long double)offset);
        startLocal[axis] = (float)(
            (long double)start[axis] + (long double)offset);
        endLocal[axis] = (float)(
            (long double)end[axis] + (long double)offset);
    }

    for (int32_t axis = 0; axis < 3; ++axis) {
        startLocal[axis] = (float)(
            (long double)startLocal[axis] - (long double)origin[axis]);
        endLocal[axis] = (float)(
            (long double)endLocal[axis] - (long double)origin[axis]);
    }

    const qboolean rotated =
        modelHandle != CM_TEMP_BOX_MODEL_HANDLE &&
        (angles[0] != 0.0f || angles[1] != 0.0f || angles[2] != 0.0f);

    /* Stock initializes only +0x00..+0x17 before CM_Trace copies the complete
     * 0x24-byte record.  The three trailing dwords are overwritten before a
     * moving trace reads them and are unused by stationary paths.  Zero the
     * dead lanes so portable C never evaluates indeterminate float objects. */
    cmTraceSphereRecord_t sphere = {0};
    sphere.sphere.use = capsule;
    sphere.sphere.radius =
        traceMaxs[2] < traceMaxs[0] ? traceMaxs[2] : traceMaxs[0];
    sphere.sphere.halfheight = traceMaxs[2];
    const float cylinderOffset = (float)(
        (long double)traceMaxs[2] -
        (long double)sphere.sphere.radius);

    axis_t matrix;
    const float (*const matrixReadOnly)[3] =
        (const float (*)[3])matrix;
    if (rotated != qfalse) {
        CreateRotationMatrix(angles, matrix);
        /* Input rotation adds X/Y before Z. Normal rotation below uses the
         * separate RotatePoint operation order. Preserve the input lanes. */
        const vec3_t unrotatedStart = {startLocal[0], startLocal[1], startLocal[2]};
        const vec3_t unrotatedEnd = {endLocal[0], endLocal[1], endLocal[2]};
        for (int32_t row = 0; row < 3; ++row) {
            startLocal[row] = (float)(
                ((long double)matrixReadOnly[row][0] * unrotatedStart[0] +
                 (long double)matrixReadOnly[row][1] * unrotatedStart[1]) +
                (long double)matrixReadOnly[row][2] * unrotatedStart[2]);
            endLocal[row] = (float)(
                ((long double)matrixReadOnly[row][0] * unrotatedEnd[0] +
                 (long double)matrixReadOnly[row][1] * unrotatedEnd[1]) +
                (long double)matrixReadOnly[row][2] * unrotatedEnd[2]);
        }
        sphere.sphere.offset[0] = (float)(
            (long double)matrix[0][2] * (long double)cylinderOffset);
        sphere.sphere.offset[1] = (float)(
            -(long double)matrix[1][2] * (long double)cylinderOffset);
        sphere.sphere.offset[2] = (float)(
            (long double)matrix[2][2] * (long double)cylinderOffset);
    } else {
        sphere.sphere.offset[0] = 0.0f;
        sphere.sphere.offset[1] = 0.0f;
        sphere.sphere.offset[2] = cylinderOffset;
    }

    trace_t localTrace;
    localTrace.fraction = trace->fraction;
    CM_Trace(&localTrace, startLocal, endLocal, traceMins, traceMaxs,
             modelHandle, contentMask, capsule, &sphere);

    const qboolean closer =
        localTrace.fraction < trace->fraction ? qtrue : qfalse;
    if (rotated != qfalse && closer != qfalse) {
        axis_t transpose;
        TransposeMatrix(matrixReadOnly, transpose);
        RotatePoint(localTrace.normal,
                    (const float (*)[3])transpose);
    }

    for (int32_t axis = 0; axis < 3; ++axis) {
        localTrace.endpos[axis] = (float)(
            ((long double)end[axis] - (long double)start[axis]) *
                (long double)localTrace.fraction +
            (long double)start[axis]);
    }
    *trace = localTrace;
}

void CM_TransformedBoxTraceExternal(trace_t *trace,
                                    const vec3_t start, const vec3_t end,
                                    const vec3_t mins, const vec3_t maxs,
                                    int32_t modelHandle, int32_t contentMask,
                                    const vec3_t origin, const vec3_t angles,
                                    qboolean capsule)
{
    trace->fraction = 1.0f;
    CM_TransformedBoxTrace(trace, start, end, mins, maxs,
                           modelHandle, contentMask,
                           origin, angles, capsule);
}

int32_t CM_BoxSightTrace(int32_t previousHit,
                         const vec3_t start, const vec3_t end,
                         const vec3_t mins, const vec3_t maxs,
                         int32_t modelHandle, int32_t contentMask,
                         qboolean capsule)
{
    static const vec3_t origin = {0.0f, 0.0f, 0.0f};
    return CM_SightTrace(previousHit, start, end, mins, maxs,
                         modelHandle, origin, contentMask, capsule, NULL);
}

int32_t CM_TransformedBoxSightTrace(int32_t previousHit,
                                    const vec3_t start, const vec3_t end,
                                    const vec3_t mins, const vec3_t maxs,
                                    int32_t modelHandle, int32_t contentMask,
                                    const vec3_t origin, const vec3_t angles,
                                    qboolean capsule)
{
    static const vec3_t zero = {0.0f, 0.0f, 0.0f};
    const float *const inputMins = mins != NULL ? mins : zero;
    const float *const inputMaxs = maxs != NULL ? maxs : zero;
    vec3_t traceMins;
    vec3_t traceMaxs;
    vec3_t startLocal;
    vec3_t endLocal;

    for (int32_t axis = 0; axis < 3; ++axis) {
        const float offset = (float)(
            ((long double)inputMins[axis] +
             (long double)inputMaxs[axis]) * 0.5L);
        traceMins[axis] = (float)(
            (long double)inputMins[axis] - (long double)offset);
        traceMaxs[axis] = (float)(
            (long double)inputMaxs[axis] - (long double)offset);
        startLocal[axis] = (float)(
            (long double)start[axis] + (long double)offset);
        endLocal[axis] = (float)(
            (long double)end[axis] + (long double)offset);
    }

    for (int32_t axis = 0; axis < 3; ++axis) {
        startLocal[axis] = (float)(
            (long double)startLocal[axis] - (long double)origin[axis]);
        endLocal[axis] = (float)(
            (long double)endLocal[axis] - (long double)origin[axis]);
    }

    const qboolean rotated =
        modelHandle != CM_TEMP_BOX_MODEL_HANDLE &&
        (angles[0] != 0.0f || angles[1] != 0.0f || angles[2] != 0.0f);

    /* As in CM_TransformedBoxTrace, the original initializes only the first
     * six dwords.  CM_SightTrace overwrites the copied trailing lanes before
     * their first read, so zero them to keep portable C defined. */
    cmTraceSphereRecord_t sphere = {0};
    sphere.sphere.use = capsule;
    sphere.sphere.radius =
        traceMaxs[2] < traceMaxs[0] ? traceMaxs[2] : traceMaxs[0];
    sphere.sphere.halfheight = traceMaxs[2];
    const float cylinderOffset = (float)(
        (long double)traceMaxs[2] -
        (long double)sphere.sphere.radius);

    if (rotated != qfalse) {
        axis_t matrix;
        const float (*const matrixReadOnly)[3] =
            (const float (*)[3])matrix;
        CreateRotationMatrix(angles, matrix);
        /* Input rotation adds X/Y before Z, retaining all unrotated lanes
         * until both endpoints have been transformed. */
        const vec3_t unrotatedStart = {startLocal[0], startLocal[1], startLocal[2]};
        const vec3_t unrotatedEnd = {endLocal[0], endLocal[1], endLocal[2]};
        for (int32_t row = 0; row < 3; ++row) {
            startLocal[row] = (float)(
                ((long double)matrixReadOnly[row][0] * unrotatedStart[0] +
                 (long double)matrixReadOnly[row][1] * unrotatedStart[1]) +
                (long double)matrixReadOnly[row][2] * unrotatedStart[2]);
            endLocal[row] = (float)(
                ((long double)matrixReadOnly[row][0] * unrotatedEnd[0] +
                 (long double)matrixReadOnly[row][1] * unrotatedEnd[1]) +
                (long double)matrixReadOnly[row][2] * unrotatedEnd[2]);
        }
        sphere.sphere.offset[0] = (float)(
            (long double)matrix[0][2] * (long double)cylinderOffset);
        sphere.sphere.offset[1] = (float)(
            -(long double)matrix[1][2] * (long double)cylinderOffset);
        sphere.sphere.offset[2] = (float)(
            (long double)matrix[2][2] * (long double)cylinderOffset);
    } else {
        sphere.sphere.offset[0] = 0.0f;
        sphere.sphere.offset[1] = 0.0f;
        sphere.sphere.offset[2] = cylinderOffset;
    }

    return CM_SightTrace(previousHit, startLocal, endLocal,
                         traceMins, traceMaxs, modelHandle, origin,
                         contentMask, capsule, &sphere);
}
