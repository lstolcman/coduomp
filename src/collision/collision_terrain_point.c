#include "collision_terrain_trace.h"

#include <math.h>

/*
 * Point-through-triangle-soup trace:
 *
 *   CoDUOMP.exe  0x004247c0..0x0042492e
 *   coduo_lnxded 0x08055595..0x080558dc
 *
 * The complete authoritative bodies retain their original operation graphs.
 */
/* Exact original single-precision terrain-triangle tolerances:
 * 0x005b9b8c = 0xba83126f and 0x005b9cbc = 0x3f8020c5. */
static const float CM_TERRAIN_TRIANGLE_COORDINATE_MIN =
    -0.0010000000474974513f;
static const float CM_TERRAIN_TRIANGLE_COORDINATE_MAX =
    1.0010000467300415f;

/* Source: CoDUOMP.exe 0x004247c0..0x0042492e.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_004247c0_0042492e.mcode.
 * Name: exact same-module Mac symbol CM_TracePointThroughTerrainCollide.
 * A crossing of a triangle plane becomes a hit only when both stored
 * barycentric coordinates and their sum remain within the original
 * 0.001-unit edge tolerance. */
void CM_TracePointThroughTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide)
{
    for (int32_t triangleIndex = 0;
         triangleIndex <
             (int32_t)terrainCollide->triangleCount;
         ++triangleIndex) {
        const collisionSoupTriangle_t *const
            triangle =
                &terrainCollide
                     ->triangles[triangleIndex];

        const long double endDistanceRaw =
            ((long double)triangle->plane.components.normal[1] *
                 traceWork->end[1] +
             (long double)triangle->plane.components.normal[0] *
                 traceWork->end[0]) +
            (long double)triangle->plane.components.normal[2] *
                traceWork->end[2] -
            triangle->plane.components.distance;
        const float endDistance =
            (float)endDistanceRaw;
        if (!(endDistanceRaw < 0.0f ||
              isunordered(endDistanceRaw, 0.0f))) {
            continue;
        }

        const long double startDistanceRaw =
            ((long double)triangle->plane.components.normal[2] *
                 traceWork->start[2] +
             (long double)triangle->plane.components.normal[1] *
                 traceWork->start[1]) +
            (long double)triangle->plane.components.normal[0] *
                traceWork->start[0] -
            triangle->plane.components.distance;
        const float startDistance =
            (float)startDistanceRaw;
        if (!(startDistanceRaw > 0.0f ||
              isunordered(startDistanceRaw, 0.0f))) {
            continue;
        }

        const long double distanceDelta =
            (long double)startDistance -
            endDistance;
        const long double enterFractionRaw =
            ((long double)startDistance - 0.125f) /
            distanceDelta;
        const float enterFraction =
            (float)enterFractionRaw;
        /* FST records the fraction as a float for a later hit, but the
         * immediately following FCOMP still sees the unrounded ST0 value. */
        if (!(enterFractionRaw <
                  traceWork->trace.fraction ||
              isunordered(
                  enterFractionRaw,
                  traceWork->trace.fraction))) {
            continue;
        }

        const long double planeFraction =
            (long double)startDistance /
            distanceDelta;
        vec3_t hitPoint;
        for (int32_t axis = 0; axis < 2;
             ++axis) {
            hitPoint[axis] = (float)(
                (long double)traceWork->start[axis] +
                planeFraction *
                    traceWork->delta[axis]);
        }
        const long double hitPointZRaw =
            (long double)traceWork->start[2] +
            planeFraction * traceWork->delta[2];
        hitPoint[2] = (float)hitPointZRaw;

        const long double sCoordinateRaw =
            hitPointZRaw *
                triangle->svec[2] +
            ((long double)hitPoint[1] *
                 triangle->svec[1] +
             (long double)hitPoint[0] *
                 triangle->svec[0]) -
            triangle->svec[3];
        const float sCoordinate =
            (float)sCoordinateRaw;
        /* The lower-bound comparison consumes the value retained by FST;
         * the upper-bound and later sum reload the rounded float. */
        if ((!isunordered(
                 sCoordinateRaw,
                 CM_TERRAIN_TRIANGLE_COORDINATE_MIN) &&
             sCoordinateRaw <
                 CM_TERRAIN_TRIANGLE_COORDINATE_MIN) ||
            (!isunordered(
                 sCoordinate,
                 CM_TERRAIN_TRIANGLE_COORDINATE_MAX) &&
             sCoordinate >
                 CM_TERRAIN_TRIANGLE_COORDINATE_MAX)) {
            continue;
        }

        const long double tCoordinate =
            (long double)hitPoint[2] *
                triangle->tvec[2] +
            ((long double)hitPoint[1] *
                 triangle->tvec[1] +
             (long double)hitPoint[0] *
                 triangle->tvec[0]) -
            triangle->tvec[3];
        if ((!isunordered(
                 tCoordinate,
                 CM_TERRAIN_TRIANGLE_COORDINATE_MIN) &&
             tCoordinate <
                 CM_TERRAIN_TRIANGLE_COORDINATE_MIN) ||
            (!isunordered(
                 (long double)sCoordinate +
                     tCoordinate,
                 CM_TERRAIN_TRIANGLE_COORDINATE_MAX) &&
             (long double)sCoordinate +
                     tCoordinate >
                 CM_TERRAIN_TRIANGLE_COORDINATE_MAX)) {
            continue;
        }

        traceWork->trace.fraction =
            enterFraction;
        traceWork->trace.normal[0] =
            triangle->plane.components.normal[0];
        traceWork->trace.normal[1] =
            triangle->plane.components.normal[1];
        traceWork->trace.normal[2] =
            triangle->plane.components.normal[2];
    }
}
