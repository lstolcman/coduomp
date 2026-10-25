#include "collision_tree_traces.h"

#include "collision_leaf_queries.h"
#include "collision_leaf_traces.h"

#include <stdint.h>

extern collisionLeaf_t *cm_leafs;
extern collisionNode_t *cm_nodes;
extern int32_t cm_checkcount;

/*
 * Complete BSP trace-traversal cluster:
 *
 *   CoDUOMP.exe  CM_PositionTest          0x004266f0..0x00426814
 *                CM_TraceThroughTree      0x00427bd0..0x00427eca
 *                CM_SightTraceThroughTree 0x00429820..0x00429b12
 *   coduo_lnxded CM_PositionTest          0x080589fe..0x08058b84
 *                CM_TraceThroughTree      0x0805a58f..0x0805a9d4
 *                CM_SightTraceThroughTree 0x0805c90a..0x0805cd61
 *
 * Both builds traverse the same nodes and leaves in the same order. Their
 * bodies retain the platform-specific x87 operation graphs and spill points.
 */
/* Source: CoDUOMP.exe 0x004266f0..0x00426814.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004266f0_00426815.mcode.
 * Name: same-family Linux symbol CM_PositionTest. Stationary traces
 * enumerate the BSP leaves touched by the start-position bounds, expanded by
 * one unit, then test each leaf until one proves the trace wholly solid. */
void CM_PositionTest(
    traceWork_t *traceWork)
{
    enum {
        CM_TEST_TRACE_MAX_LEAVES = 1024
    };

    cmLeafQueryWork_t leafWork;
    int32_t leafList[CM_TEST_TRACE_MAX_LEAVES];

    leafWork.count = 0;
    leafWork.maxCount =
        CM_TEST_TRACE_MAX_LEAVES;
    leafWork.overflowed = qfalse;
    leafWork.items = leafList;
    for (int32_t axis = 0; axis < 3; ++axis) {
        leafWork.mins[axis] =
            traceWork->start[axis] +
            traceWork->mins[axis] -
            1.0f;
        leafWork.maxs[axis] =
            traceWork->start[axis] +
            traceWork->maxs[axis] +
            1.0f;
    }
    leafWork.lastLeaf = 0;
    leafWork.storeLeafs = CM_StoreLeafs;

    cm_checkcount++;
    CM_BoxLeafnums_r(&leafWork, 0);
    cm_checkcount++;

    for (int32_t leafIndex = 0;
         leafIndex < leafWork.count;
         ++leafIndex) {
        CM_TestInLeaf(
            traceWork,
            &cm_leafs[leafList[leafIndex]]);
        if (traceWork->trace.allsolid != qfalse)
            return;
    }
}

/* Source: CoDUOMP.exe 0x00427bd0..0x00427eca.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00427bd0_00427ecb.mcode.
 * Name: same-family Linux symbol CM_TraceThroughTree. Recursively split a
 * moving trace at BSP planes, preserving the original y+z+x non-axial dot
 * product order and the two independently clamped child fractions. */
void CM_TraceThroughTree(
    traceWork_t *traceWork, int32_t nodeNum,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    vec3_t currentStart = {start[0], start[1], start[2]};
    const float *const currentEnd = end;

    /* Stock 0x00427cca, 0x00427d15, and 0x00427ebb immediately
     * return after their recursive child calls. Reuse this native frame for
     * those proven tail calls; the non-tail near-child call at 0x00427e30
     * remains recursive so leaf order and fraction updates stay unchanged. */
    for (;;) {
        if (traceWork->trace.fraction <=
            startFraction) {
            return;
        }

        if (nodeNum < 0) {
            CM_TraceThroughLeaf(
                traceWork, &cm_leafs[-1 - nodeNum]);
            return;
        }

        const collisionNode_t *const node =
            &cm_nodes[nodeNum];
        const cplane_t *const plane = node->plane;
        long double startDistanceRaw;
        float endDistance;
        long double offsetRaw;
        const float nonAxialOffset = 2048.0f;
        const float planePadding = 1.0f;
        const float splitEpsilon = 0.125f;

        /* The start distance remains live; only the end distance is stored
         * before the child comparisons and split calculations. */
        if (plane->type < 3) {
            startDistanceRaw =
                (long double)currentStart[plane->type] -
                plane->dist;
            endDistance = (float)((long double)currentEnd[plane->type] - plane->dist);
            offsetRaw = traceWork->maxs[plane->type];
        } else {
            startDistanceRaw =
                (((long double)plane->normal[1] * currentStart[1] +
                  (long double)plane->normal[2] * currentStart[2]) +
                 (long double)plane->normal[0] * currentStart[0]) -
                plane->dist;
            endDistance = (float)(
                (((long double)plane->normal[1] * currentEnd[1] +
                  (long double)plane->normal[2] * currentEnd[2]) +
                 (long double)plane->normal[0] * currentEnd[0]) -
                plane->dist);
            offsetRaw =
                traceWork->isPoint == qfalse
                    ? nonAxialOffset
                    : 0.0f;
        }

        if (startDistanceRaw >= offsetRaw + planePadding &&
            (long double)endDistance >= offsetRaw + planePadding) {
            nodeNum = node->children[0];
            continue;
        }

        if (startDistanceRaw < -offsetRaw - planePadding &&
            (long double)endDistance < -offsetRaw - planePadding) {
            nodeNum = node->children[1];
            continue;
        }

        int32_t side;
        long double firstFractionRaw;
        float secondFraction;

        /* The reciprocal and near-child fraction remain wide. The far-child
         * fraction is rounded before the near-child call. */
        if (startDistanceRaw < (long double)endDistance) {
            const long double inverseDistanceRaw = 1.0L / (startDistanceRaw - endDistance);
            side = 1;
            secondFraction = (float)((startDistanceRaw + offsetRaw + splitEpsilon) * inverseDistanceRaw);
            firstFractionRaw = (startDistanceRaw - offsetRaw + splitEpsilon) * inverseDistanceRaw;
        } else if ((long double)endDistance < startDistanceRaw) {
            const long double inverseDistanceRaw = 1.0L / (startDistanceRaw - endDistance);
            side = 0;
            secondFraction = (float)((startDistanceRaw - offsetRaw - splitEpsilon) * inverseDistanceRaw);
            firstFractionRaw = (startDistanceRaw + offsetRaw + splitEpsilon) * inverseDistanceRaw;
        } else {
            side = 0;
            firstFractionRaw = 1.0L;
            secondFraction = 0.0f;
        }

        if (firstFractionRaw < 0.0L)
            firstFractionRaw = 0.0L;
        if (firstFractionRaw > 1.0L)
            firstFractionRaw = 1.0L;

        /* Both child intervals consume the same stored fraction span. */
        const float fractionSpan = (float)((long double)endFraction - startFraction);
        const float firstMidFraction = (float)((long double)startFraction + fractionSpan * firstFractionRaw);
        vec3_t mid;
        for (int32_t axis = 0; axis < 3; ++axis) {
            mid[axis] = (float)(
                (long double)currentStart[axis] +
                ((long double)currentEnd[axis] - currentStart[axis]) * firstFractionRaw);
        }

        CM_TraceThroughTree(
            traceWork, node->children[side],
            startFraction, firstMidFraction,
            currentStart, mid);

        if (secondFraction < 0.0f)
            secondFraction = 0.0f;
        if (secondFraction > 1.0f)
            secondFraction = 1.0f;

        const float secondMidFraction = (float)(
            (long double)startFraction + (long double)fractionSpan * secondFraction);
        for (int32_t axis = 0; axis < 3; ++axis) {
            mid[axis] = (float)(
                (long double)currentStart[axis] +
                ((long double)currentEnd[axis] - currentStart[axis]) * secondFraction);
        }

        nodeNum = node->children[side ^ 1];
        startFraction = secondMidFraction;
        currentStart[0] = mid[0];
        currentStart[1] = mid[1];
        currentStart[2] = mid[2];
    }
}
/* Source: CoDUOMP.exe 0x00429820..0x00429b12.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00429820_00429b13.mcode.
 * Name: exact same-module Mac symbol CM_SightTraceThroughTree. Recursively
 * split the sight segment at BSP planes, visit the near child first, and
 * propagate the first nonzero brush or terrain hit number. */
int32_t CM_SightTraceThroughTree(
    const traceWork_t *traceWork,
    int32_t nodeNum,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    if (nodeNum < 0) {
        return CM_SightTraceThroughLeaf(
            traceWork,
            &cm_leafs[-1 - nodeNum]);
    }

    const collisionNode_t *const node =
        &cm_nodes[nodeNum];
    const cplane_t *const plane =
        node->plane;
    long double startDistanceRaw;
    float endDistance;
    long double offsetRaw;
    const float nonAxialOffset = 2048.0f;

    if (plane->type < 3) {
        startDistanceRaw =
            (long double)start[plane->type] -
            plane->dist;
        endDistance =
            end[plane->type] -
            plane->dist;
        offsetRaw =
            traceWork->maxs[plane->type];
    } else {
        startDistanceRaw =
            ((long double)plane->normal[1] * start[1] +
             (long double)plane->normal[2] * start[2]) +
            (long double)plane->normal[0] * start[0] -
            plane->dist;
        endDistance = (float)(
            ((long double)plane->normal[1] * end[1] +
             (long double)plane->normal[2] * end[2]) +
            (long double)plane->normal[0] * end[0] -
            plane->dist);
        offsetRaw =
            traceWork->isPoint == qfalse
                ? nonAxialOffset
                : 0.0f;
    }

    if (startDistanceRaw >= offsetRaw + 1.0f &&
        (long double)endDistance >=
            offsetRaw + 1.0f) {
        return CM_SightTraceThroughTree(
            traceWork, node->children[0],
            startFraction, endFraction,
            start, end);
    }

    if (startDistanceRaw < -1.0f - offsetRaw &&
        (long double)endDistance <
            -1.0f - offsetRaw) {
        return CM_SightTraceThroughTree(
            traceWork, node->children[1],
            startFraction, endFraction,
            start, end);
    }

    int32_t side;
    float firstFraction;
    long double secondFractionRaw;
    const float splitEpsilon = 0.125f;
    if (startDistanceRaw <
        (long double)endDistance) {
        const long double inverseDistanceRaw =
            1.0f /
            (startDistanceRaw - endDistance);
        side = 1;
        firstFraction = (float)(
            (startDistanceRaw + offsetRaw +
             splitEpsilon) *
            inverseDistanceRaw);
        secondFractionRaw =
            (startDistanceRaw - offsetRaw +
             splitEpsilon) *
            inverseDistanceRaw;
    } else if ((long double)endDistance <
               startDistanceRaw) {
        const long double inverseDistanceRaw =
            1.0f /
            (startDistanceRaw - endDistance);
        side = 0;
        firstFraction = (float)(
            (startDistanceRaw - offsetRaw -
             splitEpsilon) *
            inverseDistanceRaw);
        secondFractionRaw =
            (startDistanceRaw + offsetRaw +
             splitEpsilon) *
            inverseDistanceRaw;
    } else {
        side = 0;
        firstFraction = 0.0f;
        secondFractionRaw = 1.0f;
    }

    if (secondFractionRaw < 0.0f)
        secondFractionRaw = 0.0f;
    if (secondFractionRaw > 1.0f)
        secondFractionRaw = 1.0f;

    const float secondMidFraction = (float)(
        (long double)startFraction +
        ((long double)endFraction -
         startFraction) *
            secondFractionRaw);
    vec3_t mid;
    for (int32_t axis = 0; axis < 3;
         ++axis) {
        mid[axis] = (float)(
            (long double)start[axis] +
            ((long double)end[axis] -
             start[axis]) *
                secondFractionRaw);
    }

    const int32_t sightHit =
        CM_SightTraceThroughTree(
            traceWork, node->children[side],
            startFraction, secondMidFraction,
            start, mid);
    if (sightHit != 0)
        return sightHit;

    if (firstFraction < 0.0f)
        firstFraction = 0.0f;
    if (firstFraction > 1.0f)
        firstFraction = 1.0f;

    const float firstMidFraction =
        startFraction +
        (endFraction - startFraction) *
            firstFraction;
    for (int32_t axis = 0; axis < 3;
         ++axis) {
        mid[axis] =
            start[axis] +
            (end[axis] - start[axis]) *
                firstFraction;
    }

    return CM_SightTraceThroughTree(
        traceWork, node->children[side ^ 1],
        firstMidFraction, endFraction,
        mid, end);
}
