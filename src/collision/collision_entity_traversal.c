#include "collision_entity_traversal.h"

#include "collision_server_trace.h"

/*
 * Complete dynamic-entity world-sector traversal cluster shared by the
 * Windows client/listen server and Linux dedicated server.  The exact original
 * names come from the Mac engine symbols; the stripped Linux recovery's
 * SV_TraceEntities, SV_SweptSightTraceEntities, SV_SightTraceEntities, and
 * SV_PointSightTraceEntities names described the same four CM-owned walks.
 *
 * Linux authoritative bodies:
 *   CM_ClipMoveToEntities_r          0x0805e8e5..0x0805ec44
 *   CM_ClipMoveToEntities            0x0805ec45..0x0805ec85
 *   CM_ClipSightTraceToEntities_r    0x0805ec86..0x0805f044
 *   CM_ClipSightTraceToEntities      0x0805f045..0x0805f086
 *   CM_PointTraceToEntities_r        0x0805f087..0x0805f2bd
 *   CM_PointTraceToEntities          0x0805f2be..0x0805f2fb
 *   CM_PointSightTraceToEntities_r   0x0805f2fc..0x0805f58b
 *   CM_PointSightTraceToEntities     0x0805f58c..0x0805f5cd
 *
 * Both platforms agree on masks, child selection, traversal order, linked
 * entity order, early exits, and callback contracts.  Separate whole-platform
 * bodies retain the observed x87 spill points: the optimized Windows bodies
 * keep selected split values live at PC=53, while Linux stores the corresponding
 * intermediates as binary32 under its PC=64 process policy.
 */

/* Source: CoDUOMP.exe 0x0042b0d0..0x0042b32e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042b0d0_0042b32f.mcode.
 * Name: exact same-module Mac symbol CM_ClipMoveToEntities_r. The swept
 * bounds expand each sector plane by expandedHalfSize before recursion;
 * linked dynamic entities in every visited sector are then clipped. */
void CM_ClipMoveToEntities_r(
    cmClipMoveWork_t *work,
    worldSector_t *sector,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    /* Equality stops the walk, while an unordered comparison continues. */
    if (work->bestTrace.fraction <= startFraction ||
        (work->contentsMask &
         sector->entityContentsMask) == 0) {
        return;
    }

    const int32_t axis = sector->axis;
    const float startDistance =
        start[axis] - sector->dist;
    const float endDistance =
        end[axis] - sector->dist;
    const float radius =
        work->expandedHalfSize[axis];

    if (startDistance >= radius &&
        endDistance >= radius) {
        CM_ClipMoveToEntities_r(
            work, sector->children[0],
            startFraction, endFraction,
            start, end);
    } else if (startDistance <= -radius &&
               endDistance <= -radius) {
        CM_ClipMoveToEntities_r(
            work, sector->children[1],
            startFraction, endFraction,
            start, end);
    } else {
        int32_t side;
        float enterFraction;
        long double leaveFractionRaw;

        if (startDistance < endDistance) {
            const long double inverseDistanceRaw =
                1.0L /
                ((long double)startDistance -
                 endDistance);
            side = 1;
            enterFraction = (float)(
                ((long double)startDistance +
                 radius) *
                inverseDistanceRaw);
            leaveFractionRaw =
                ((long double)startDistance -
                 radius) *
                inverseDistanceRaw;
        } else if (endDistance < startDistance) {
            const long double inverseDistanceRaw =
                1.0L /
                ((long double)startDistance -
                 endDistance);
            side = 0;
            enterFraction = (float)(
                ((long double)startDistance -
                 radius) *
                inverseDistanceRaw);
            leaveFractionRaw =
                ((long double)startDistance +
                 radius) *
                inverseDistanceRaw;
        } else {
            side = 0;
            enterFraction = 0.0f;
            leaveFractionRaw = 1.0L;
        }

        if (leaveFractionRaw > 1.0L)
            leaveFractionRaw = 1.0L;

        vec3_t middle = {
            (float)((long double)start[0] +
                    ((long double)end[0] -
                     start[0]) *
                        leaveFractionRaw),
            (float)((long double)start[1] +
                    ((long double)end[1] -
                     start[1]) *
                        leaveFractionRaw),
            (float)((long double)start[2] +
                    ((long double)end[2] -
                     start[2]) *
                        leaveFractionRaw)
        };
        const float fractionSpan = (float)(
            (long double)endFraction -
            startFraction);
        const float leaveMiddleFraction = (float)(
            (long double)startFraction +
            (long double)fractionSpan *
                leaveFractionRaw);

        CM_ClipMoveToEntities_r(
            work, sector->children[side],
            startFraction, leaveMiddleFraction,
            start, middle);

        if (enterFraction < 0.0f)
            enterFraction = 0.0f;

        middle[0] = (float)(
            (long double)start[0] +
            ((long double)end[0] -
             start[0]) *
                enterFraction);
        middle[1] = (float)(
            (long double)start[1] +
            ((long double)end[1] -
             start[1]) *
                enterFraction);
        middle[2] = (float)(
            (long double)start[2] +
            ((long double)end[2] -
             start[2]) *
                enterFraction);
        const float enterMiddleFraction = (float)(
            (long double)startFraction +
            (long double)fractionSpan *
                enterFraction);

        CM_ClipMoveToEntities_r(
            work, sector->children[1 - side],
            enterMiddleFraction, endFraction,
            middle, end);
    }

    for (svEntity_t *serverEntity =
             sector->entityLinkHead;
         serverEntity != NULL;
         serverEntity =
             serverEntity->nextInWorldSector) {
        SV_ClipMoveToEntity(work, serverEntity);
    }
}

/* Source: CoDUOMP.exe 0x0042b330..0x0042b34c.
 * Name: exact same-module Mac symbol CM_ClipMoveToEntities. The recursive
 * end-fraction bound is the current best world-trace fraction, not 1.0. */
void CM_ClipMoveToEntities(cmClipMoveWork_t *work)
{
    CM_ClipMoveToEntities_r(
        work, &cm_worldSectorRoot,
        0.0f, work->bestTrace.fraction,
        work->start, work->end);
}

/* Source: CoDUOMP.exe 0x0042b350..0x0042b5b5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042b350_0042b5b6.mcode.
 * Name: exact same-module Mac symbol CM_ClipSightTraceToEntities_r. This is
 * the swept sight-query counterpart of CM_ClipMoveToEntities_r and returns
 * immediately when either a child sector or linked entity reports a hit. */
int32_t CM_ClipSightTraceToEntities_r(
    cmClipSightTraceWork_t *work,
    worldSector_t *sector,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    if ((work->contentsMask &
         sector->entityContentsMask) == 0) {
        return 0;
    }

    const int32_t axis = sector->axis;
    const float startDistance =
        start[axis] - sector->dist;
    const float endDistance =
        end[axis] - sector->dist;
    const float radius =
        work->expandedHalfSize[axis];
    int32_t hit;

    if (startDistance >= radius &&
        endDistance >= radius) {
        hit = CM_ClipSightTraceToEntities_r(
            work, sector->children[0],
            startFraction, endFraction,
            start, end);
    } else if (startDistance <= -radius &&
               endDistance <= -radius) {
        hit = CM_ClipSightTraceToEntities_r(
            work, sector->children[1],
            startFraction, endFraction,
            start, end);
    } else {
        int32_t side;
        float enterFraction;
        long double leaveFractionRaw;

        if (startDistance < endDistance) {
            const long double inverseDistanceRaw =
                1.0L /
                ((long double)startDistance -
                 endDistance);
            side = 1;
            enterFraction = (float)(
                ((long double)startDistance +
                 radius) *
                inverseDistanceRaw);
            leaveFractionRaw =
                ((long double)startDistance -
                 radius) *
                inverseDistanceRaw;
        } else if (endDistance < startDistance) {
            const long double inverseDistanceRaw =
                1.0L /
                ((long double)startDistance -
                 endDistance);
            side = 0;
            enterFraction = (float)(
                ((long double)startDistance -
                 radius) *
                inverseDistanceRaw);
            leaveFractionRaw =
                ((long double)startDistance +
                 radius) *
                inverseDistanceRaw;
        } else {
            side = 0;
            enterFraction = 0.0f;
            leaveFractionRaw = 1.0L;
        }

        if (leaveFractionRaw > 1.0L)
            leaveFractionRaw = 1.0L;

        vec3_t middle = {
            (float)((long double)start[0] +
                    ((long double)end[0] -
                     start[0]) *
                        leaveFractionRaw),
            (float)((long double)start[1] +
                    ((long double)end[1] -
                     start[1]) *
                        leaveFractionRaw),
            (float)((long double)start[2] +
                    ((long double)end[2] -
                     start[2]) *
                        leaveFractionRaw)
        };
        const float fractionSpan = (float)(
            (long double)endFraction -
            startFraction);
        const float leaveMiddleFraction = (float)(
            (long double)startFraction +
            (long double)fractionSpan *
                leaveFractionRaw);

        hit = CM_ClipSightTraceToEntities_r(
            work, sector->children[side],
            startFraction, leaveMiddleFraction,
            start, middle);
        if (hit != 0)
            return hit;

        if (enterFraction < 0.0f)
            enterFraction = 0.0f;

        middle[0] = (float)(
            (long double)start[0] +
            ((long double)end[0] -
             start[0]) *
                enterFraction);
        middle[1] = (float)(
            (long double)start[1] +
            ((long double)end[1] -
             start[1]) *
                enterFraction);
        middle[2] = (float)(
            (long double)start[2] +
            ((long double)end[2] -
             start[2]) *
                enterFraction);
        const float enterMiddleFraction = (float)(
            (long double)startFraction +
            (long double)fractionSpan *
                enterFraction);

        hit = CM_ClipSightTraceToEntities_r(
            work, sector->children[1 - side],
            enterMiddleFraction, endFraction,
            middle, end);
    }

    if (hit != 0)
        return hit;

    for (svEntity_t *serverEntity =
             sector->entityLinkHead;
         serverEntity != NULL;
         serverEntity =
             serverEntity->nextInWorldSector) {
        hit = SV_ClipSightTraceToEntity(
            work, serverEntity);
        if (hit != 0)
            return hit;
    }

    return 0;
}

/* Source: CoDUOMP.exe 0x0042b5c0..0x0042b5dd.
 * Name: exact same-module Mac symbol CM_ClipSightTraceToEntities. */
int32_t CM_ClipSightTraceToEntities(cmClipSightTraceWork_t *work)
{
    return CM_ClipSightTraceToEntities_r(
        work, &cm_worldSectorRoot,
        0.0f, 1.0f, work->start, work->end);
}

/* Source: CoDUOMP.exe 0x0042b5e0..0x0042b767.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042b5e0_0042b768.mcode.
 * Name: exact same-module Mac symbol CM_PointTraceToEntities_r. Point traces
 * split directly on the sector plane and retain the best-fraction pruning
 * used by the swept move walker. */
void CM_PointTraceToEntities_r(
    cmPointTraceWork_t *work,
    worldSector_t *sector,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    /* Equality stops the walk, while an unordered comparison continues. */
    if (work->bestTrace.fraction <= startFraction ||
        (work->contentsMask &
         sector->entityContentsMask) == 0) {
        return;
    }

    const int32_t axis = sector->axis;
    const long double startDistanceRaw =
        (long double)start[axis] -
        sector->dist;
    const float endDistance = (float)(
        (long double)end[axis] -
        sector->dist);

    if (startDistanceRaw >= 0.0L &&
        endDistance >= 0.0f) {
        CM_PointTraceToEntities_r(
            work, sector->children[0],
            startFraction, endFraction,
            start, end);
    } else if (startDistanceRaw <= 0.0L &&
               endDistance <= 0.0f) {
        CM_PointTraceToEntities_r(
            work, sector->children[1],
            startFraction, endFraction,
            start, end);
    } else {
        const long double splitFractionRaw =
            startDistanceRaw /
            (startDistanceRaw - endDistance);
        const float middleFraction = (float)(
            (long double)startFraction +
            ((long double)endFraction -
             startFraction) *
                splitFractionRaw);
        vec3_t middle = {
            (float)((long double)start[0] +
                    ((long double)end[0] -
                     start[0]) *
                        splitFractionRaw),
            (float)((long double)start[1] +
                    ((long double)end[1] -
                     start[1]) *
                        splitFractionRaw),
            (float)((long double)start[2] +
                    ((long double)end[2] -
                     start[2]) *
                        splitFractionRaw)
        };
        const int32_t side =
            startDistanceRaw < 0.0L ? 1 : 0;

        CM_PointTraceToEntities_r(
            work, sector->children[side],
            startFraction, middleFraction,
            start, middle);
        CM_PointTraceToEntities_r(
            work, sector->children[1 - side],
            middleFraction, endFraction,
            middle, end);
    }

    for (svEntity_t *serverEntity =
             sector->entityLinkHead;
         serverEntity != NULL;
         serverEntity =
             serverEntity->nextInWorldSector) {
        SV_PointTraceToEntity(work, serverEntity);
    }
}

/* Source: CoDUOMP.exe 0x0042b770..0x0042b789.
 * Name: exact same-module Mac symbol CM_PointTraceToEntities. Like the swept
 * wrapper, the recursion bound is the current best trace fraction. */
void CM_PointTraceToEntities(cmPointTraceWork_t *work)
{
    CM_PointTraceToEntities_r(
        work, &cm_worldSectorRoot,
        0.0f, work->bestTrace.fraction,
        work->start, work->end);
}

/* Source: CoDUOMP.exe 0x0042b790..0x0042b902.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0042b790_0042b903.mcode.
 * Name: exact same-module Mac symbol CM_PointSightTraceToEntities_r. The
 * first nonzero child or linked-entity result terminates the point sight
 * query. */
int32_t CM_PointSightTraceToEntities_r(
    cmPointSightTraceWork_t *work,
    worldSector_t *sector,
    float startFraction, float endFraction,
    const vec3_t start, const vec3_t end)
{
    if ((work->contentsMask &
         sector->entityContentsMask) == 0) {
        return 0;
    }

    const int32_t axis = sector->axis;
    const long double startDistanceRaw =
        (long double)start[axis] -
        sector->dist;
    const float endDistance = (float)(
        (long double)end[axis] -
        sector->dist);
    int32_t hit;

    if (startDistanceRaw >= 0.0L &&
        endDistance >= 0.0f) {
        hit = CM_PointSightTraceToEntities_r(
            work, sector->children[0],
            startFraction, endFraction,
            start, end);
    } else if (startDistanceRaw <= 0.0L &&
               endDistance <= 0.0f) {
        hit = CM_PointSightTraceToEntities_r(
            work, sector->children[1],
            startFraction, endFraction,
            start, end);
    } else {
        const long double splitFractionRaw =
            startDistanceRaw /
            (startDistanceRaw - endDistance);
        const float middleFraction = (float)(
            (long double)startFraction +
            ((long double)endFraction -
             startFraction) *
                splitFractionRaw);
        vec3_t middle = {
            (float)((long double)start[0] +
                    ((long double)end[0] -
                     start[0]) *
                        splitFractionRaw),
            (float)((long double)start[1] +
                    ((long double)end[1] -
                     start[1]) *
                        splitFractionRaw),
            (float)((long double)start[2] +
                    ((long double)end[2] -
                     start[2]) *
                        splitFractionRaw)
        };
        const int32_t side =
            startDistanceRaw < 0.0L ? 1 : 0;

        hit = CM_PointSightTraceToEntities_r(
            work, sector->children[side],
            startFraction, middleFraction,
            start, middle);
        if (hit != 0)
            return hit;

        hit = CM_PointSightTraceToEntities_r(
            work, sector->children[1 - side],
            middleFraction, endFraction,
            middle, end);
    }

    if (hit != 0)
        return hit;

    for (svEntity_t *serverEntity =
             sector->entityLinkHead;
         serverEntity != NULL;
         serverEntity =
             serverEntity->nextInWorldSector) {
        hit = SV_PointSightTraceToEntity(
            work, serverEntity);
        if (hit != 0)
            return hit;
    }

    return 0;
}

/* Source: CoDUOMP.exe 0x0042b910..0x0042b92a.
 * Name: exact same-module Mac symbol CM_PointSightTraceToEntities. */
int32_t CM_PointSightTraceToEntities(cmPointSightTraceWork_t *work)
{
    return CM_PointSightTraceToEntities_r(
        work, &cm_worldSectorRoot,
        0.0f, 1.0f, work->start, work->end);
}
