#include "compat/coduo_compiler.h"
#include "collision_static_model_trace.h"

#include "collision_trace_bounds.h"
#include "math/q_math.h"
#include "animation/xmodel.h"

#include <stdint.h>
#include <string.h>


/*
 * Complete static-model trace subsystem shared by the Windows client/listen
 * server and Linux dedicated server:
 *
 *   CM_TraceStaticModel
 *       CoDUOMP.exe   0x004237a0..0x00423990
 *       coduo_lnxded  0x08053d9b..0x08053fa1
 *   CM_PointTraceStaticModels_r / CM_PointTraceStaticModels
 *       CoDUOMP.exe   0x0042ae40..0x0042b0ce
 *       coduo_lnxded  0x0805e4fd..0x0805e8e4
 *   CM_SightTraceStaticModels_r / CM_SightTraceStaticModels
 *       CoDUOMP.exe   0x0042b930..0x0042bb97
 *       coduo_lnxded  0x0805f5ce..0x0805f985
 *
 * The ownership, masks, traversal order, broad-phase tests, model calls, and
 * result contract agree.  Whole platform bodies retain genuine compiler/x87
 * differences: Windows keeps recursive split values live at PC=53 and inlines
 * model transforms with its observed product order; Linux stores recursive
 * distances/fractions as binary32 at PC=64 and calls the common matrix helpers.
 */

void CM_TraceStaticModel(worldSectorAreaLink_t *areaLink,
                         trace_t *trace, const vec3_t start,
                         const vec3_t end, int32_t contentsMask)
{
    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    trace_t localTrace;
    localTrace.fraction = trace->fraction;
    localTrace.material = NULL;
    localTrace.partName = 0;
    localTrace.partGroup = 0;

    const int32_t partNameCount = XModelNumBones(areaLink->model);
    CODUO_STACK_ARRAY(DObjSkelMat, basePose, partNameCount);
    XModelGetBasePose(areaLink->model, basePose);

    vec3_t offsetStart;
    vec3_t localStart;
    vec3_t offsetEnd;
    vec3_t localEnd;

    for (int32_t axis = 0; axis < 3; ++axis) {
        offsetStart[axis] = (float)((long double)start[axis] -
                                    areaLink->origin[axis]);
        offsetEnd[axis] = (float)((long double)end[axis] -
                                  areaLink->origin[axis]);
    }
    for (int32_t lane = 0; lane < 3; ++lane) {
        localStart[lane] = (float)(
            ((long double)offsetStart[2] *
                 areaLink->inverseAxis[2][lane] +
             (long double)offsetStart[1] *
                 areaLink->inverseAxis[1][lane]) +
            (long double)offsetStart[0] *
                areaLink->inverseAxis[0][lane]);
        localEnd[lane] = (float)(
            ((long double)offsetEnd[2] *
                 areaLink->inverseAxis[2][lane] +
             (long double)offsetEnd[1] *
                 areaLink->inverseAxis[1][lane]) +
            (long double)offsetEnd[0] *
                areaLink->inverseAxis[0][lane]);
    }

    if (XModelTraceLine(areaLink->model, &localTrace, basePose,
                        localStart, localEnd, contentsMask) < 0) {
        return;
    }

    localTrace.entityNum = ENTITYNUM_WORLD;
    vec3_t delta;
    for (int32_t axis = 0; axis < 3; ++axis) {
        delta[axis] = (float)((long double)end[axis] - start[axis]);
        localTrace.endpos[axis] = (float)(
            (long double)delta[axis] * localTrace.fraction + start[axis]);
    }

    vec3_t worldNormal;
    for (int32_t row = 0; row < 3; ++row) {
        worldNormal[row] = (float)(
            ((long double)localTrace.normal[2] *
                 areaLink->inverseAxis[row][2] +
             (long double)localTrace.normal[1] *
                 areaLink->inverseAxis[row][1]) +
            (long double)localTrace.normal[0] *
                areaLink->inverseAxis[row][0]);
    }
    (void)VectorNormalize(worldNormal);
    localTrace.normal[0] = worldNormal[0];
    localTrace.normal[1] = worldNormal[1];
    localTrace.normal[2] = worldNormal[2];
    memcpy(trace, &localTrace, sizeof(localTrace));
}

void CM_PointTraceStaticModels_r(cmPointTraceStaticModelsWork_t *work,
                                 worldSector_t *sector,
                                 float startFraction, float endFraction,
                                 const vec3_t start, const vec3_t end)
{
    if (work->trace.fraction <= startFraction ||
        (work->contentsMask & sector->staticModelContentsMask) == 0) {
        return;
    }

    const int32_t axis = sector->axis;
    const long double startDistance =
        (long double)start[axis] - sector->dist;
    const float endDistance =
        (float)((long double)end[axis] - sector->dist);
    if (!(startDistance >= 0.0L) || !(endDistance >= 0.0f)) {
        if (!(startDistance <= 0.0L) || !(endDistance <= 0.0f)) {
            const long double splitFraction =
                startDistance / (startDistance - endDistance);
            const float middleFraction = (float)(
                (long double)startFraction +
                ((long double)endFraction - startFraction) * splitFraction);
            vec3_t middle;
            for (int32_t lane = 0; lane < 3; ++lane) {
                middle[lane] = (float)(
                    (long double)start[lane] +
                    ((long double)end[lane] - start[lane]) * splitFraction);
            }
            const int32_t side = startDistance < 0.0L ? 1 : 0;
            CM_PointTraceStaticModels_r(work, sector->children[side],
                                        startFraction, middleFraction,
                                        start, middle);
            CM_PointTraceStaticModels_r(work, sector->children[1 - side],
                                        middleFraction, endFraction,
                                        middle, end);
        } else {
            CM_PointTraceStaticModels_r(work, sector->children[1],
                                        startFraction, endFraction,
                                        start, end);
        }
    } else {
        CM_PointTraceStaticModels_r(work, sector->children[0],
                                    startFraction, endFraction, start, end);
    }

    for (worldSectorAreaLink_t *areaLink = sector->staticModelLinkHead;
         areaLink != NULL; areaLink = areaLink->nextInWorldSector) {
        if ((XModelGetContents(areaLink->model) & work->contentsMask) == 0 ||
            CM_TraceLineSkipsBox(work->start, work->end,
                                 areaLink->linkMins, areaLink->linkMaxs,
                                 work->trace.fraction) != qfalse) {
            continue;
        }
        CM_TraceStaticModel(areaLink, &work->trace, work->start, work->end,
                            work->contentsMask);
    }
}

void CM_PointTraceStaticModels(trace_t *trace, const vec3_t start,
                               const vec3_t end, int32_t contentsMask)
{
    cmPointTraceStaticModelsWork_t work;
    memset(&work.trace, 0, sizeof(work.trace));
    work.trace.fraction = trace->fraction;
    work.contentsMask = contentsMask;
    memcpy(work.start, start, sizeof(work.start));
    memcpy(work.end, end, sizeof(work.end));

    CM_PointTraceStaticModels_r(&work, &cm_worldSectorRoot, 0.0f,
                                work.trace.fraction, work.start, work.end);
    if (
        work.trace.fraction < trace->fraction
    ) {
        const long double deltaX = (long double)end[0] - start[0];
        const long double deltaY = (long double)end[1] - start[1];
        const float deltaZ = (float)((long double)end[2] - start[2]);
        work.trace.endpos[0] = (float)((long double)start[0] +
                                      deltaX * work.trace.fraction);
        work.trace.endpos[1] = (float)((long double)start[1] +
                                      deltaY * work.trace.fraction);
        work.trace.endpos[2] = (float)((long double)start[2] +
                                      (long double)deltaZ *
                                          work.trace.fraction);
        *trace = work.trace;
    }
}

qboolean CM_SightTraceStaticModels_r(
    cmSightTraceStaticModelsWork_t *work, worldSector_t *sector,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    if ((work->contentsMask & sector->sightTraceStaticModelContentsMask) == 0) {
        return qtrue;
    }

    const int32_t axis = sector->axis;
    const long double startDistance =
        (long double)start[axis] - sector->dist;
    const float endDistance =
        (float)((long double)end[axis] - sector->dist);
    if (startDistance >= 0.0L && endDistance >= 0.0f) {
        if (CM_SightTraceStaticModels_r(work, sector->children[0],
                                        startFraction, endFraction,
                                        start, end) == qfalse) {
            return qfalse;
        }
    } else if (startDistance <= 0.0L && endDistance <= 0.0f) {
        if (CM_SightTraceStaticModels_r(work, sector->children[1],
                                        startFraction, endFraction,
                                        start, end) == qfalse) {
            return qfalse;
        }
    } else {
        const long double splitFraction =
            startDistance / (startDistance - endDistance);
        const float middleFraction = (float)(
            (long double)startFraction +
            ((long double)endFraction - startFraction) * splitFraction);
        vec3_t middle;
        for (int32_t lane = 0; lane < 3; ++lane) {
            middle[lane] = (float)(
                (long double)start[lane] +
                ((long double)end[lane] - start[lane]) * splitFraction);
        }
        const int32_t side = startDistance < 0.0L ? 1 : 0;
        if (CM_SightTraceStaticModels_r(work, sector->children[side],
                                        startFraction, middleFraction,
                                        start, middle) == qfalse ||
            CM_SightTraceStaticModels_r(work, sector->children[1 - side],
                                        middleFraction, endFraction,
                                        middle, end) == qfalse) {
            return qfalse;
        }
    }

    trace_t trace;
    trace.fraction = 1.0f;
    for (worldSectorAreaLink_t *areaLink = sector->staticModelLinkHead;
         areaLink != NULL; areaLink = areaLink->nextInWorldSector) {
        if (areaLink->sightTraceEligible == qfalse ||
            (XModelGetContents(areaLink->model) & work->contentsMask) == 0 ||
            CM_TraceLineSkipsBox(work->start, work->end,
                                 areaLink->linkMins, areaLink->linkMaxs,
                                 1.0f) != qfalse) {
            continue;
        }
        CM_TraceStaticModel(areaLink, &trace, work->start, work->end,
                            work->contentsMask);
        if (
            trace.fraction != 1.0f
        ) {
            return qfalse;
        }
    }
    return qtrue;
}

qboolean CM_SightTraceStaticModels(const vec3_t start, const vec3_t end,
                                   int32_t contentsMask)
{
    cmSightTraceStaticModelsWork_t work;
    work.contentsMask = contentsMask;
    memcpy(work.start, start, sizeof(work.start));
    memcpy(work.end, end, sizeof(work.end));
    return CM_SightTraceStaticModels_r(&work, &cm_worldSectorRoot,
                                       0.0f, 1.0f,
                                       work.start, work.end);
}
