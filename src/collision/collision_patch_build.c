#include "collision_patch_build.h"
#include "collision_queries.h"

#include "math/q_math.h"
#include "qcommon/hunk.h"

#include <math.h>
#include <stddef.h>
#include <string.h>


/*
 * Complete curved-patch collision builder shared by the Windows client engine
 * and the Linux dedicated engine.  The retained function boundaries and
 * control flow agree across both binaries.  Whole-function behavior bodies are
 * used only where the original instruction streams differ in operation order
 * or binary32 spill points; storage duration and allocator differences stay at
 * the affected declarations and call sites.
 */

void Com_DPrintf(const char *format, ...);
void Com_Error(errorParm_t code, const char *format, ...);
void *Com_Memcpy(void *destination, const void *source, size_t count);
void *Com_Memset(void *destination, int value, size_t count);
void Com_Printf(const char *format, ...);

enum {
    CM_PATCH_BEVEL_AXIS_MIN = -1,
    CM_PATCH_BEVEL_AXIS_MAX = 1,
    CM_PATCH_EDGE_BOTTOM = 0,
    CM_PATCH_EDGE_RIGHT = 1,
    CM_PATCH_EDGE_TOP = 2,
    CM_PATCH_EDGE_LEFT = 3,
    CM_PATCH_EDGE_DIAGONAL_DESCENDING = 4,
    CM_PATCH_EDGE_DIAGONAL_ASCENDING = 5,
    CM_PATCH_BORDER_INWARD_LOWER_TRIANGLE = 0,
    CM_PATCH_BORDER_INWARD_UPPER_TRIANGLE = 1,
    CM_PATCH_BORDER_INWARD_QUAD = -1,
    CM_PATCH_HUNK_ALIGNMENT = 32,
    CM_PATCH_WORLD_COORD_LIMIT = 131072
};

/* The executable compares the normalized edge and point-plane distances to
 * double constants. ChopWindingInPlace receives the separately rounded float
 * 0.1f argument at both call sites. */
static const double CM_PATCH_BEVEL_EDGE_LENGTH_MIN = 0.5;
static const double CM_PATCH_BEVEL_PLANE_EPSILON = 0.1;
static const float CM_PATCH_CHOP_EPSILON = 0.10000000149011612f;
static const double CM_PATCH_GRID_POINT_EPSILON = 0.1;
static const double CM_PATCH_PLANE_NORMAL_EPSILON = 0.0001;
static const double CM_PATCH_PLANE_DISTANCE_EPSILON = 0.02;
static const double CM_PATCH_POINT_PLANE_EPSILON = 0.1;

/* Windows original storage: 0x008d09b0, preceded by cm_patchPlaneCount at
 * 0x008d09ac.  Both original builders populate one private 4096-plane work
 * bank per binary; the complete shared subsystem therefore owns that bank. */
patchPlane_t cm_patchPlanes[CM_PATCH_PLANE_LIMIT];
int32_t cm_patchPlaneCount;
int32_t cm_windingActiveCount; /* original 0x0494ddac */
int32_t cm_windingPeakActiveCount; /* original 0x0494ddb0 */
float cm_windingSplitDist; /* original 0x008e49b0 */
float cm_windingChopDist; /* original 0x008e49b4 */


/* NOT_FROM_ORIGINAL_SOURCE: typed factoring of the repeated four-dword plane
 * loads and optional sign flip in CM_AddFacetBevels. */
static void CM_LoadPatchFacetPlane(int32_t planeIndex, qboolean inward,
                                   vec4_t plane)
{
    plane[0] = cm_patchPlanes[planeIndex].normal[0];
    plane[1] = cm_patchPlanes[planeIndex].normal[1];
    plane[2] = cm_patchPlanes[planeIndex].normal[2];
    plane[3] = cm_patchPlanes[planeIndex].dist;

    if (inward == qfalse) {
        plane[0] = -plane[0];
        plane[1] = -plane[1];
        plane[2] = -plane[2];
        plane[3] = -plane[3];
    }
}

/* Source: CoDUOMP.exe 0x0041e310..0x0041e3ff.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041e310_0041e3ff.mcode.
 * Name: exact same-module Mac symbol CM_PlaneEqual. The normal tolerance is
 * the original double 0.0001 (0x3f1a36e2eb1c432d), and the distance tolerance
 * is double 0.02 (0x3f947ae147ae147b). Both comparisons are strict. */
qboolean CM_PlaneEqual(
    const vec4_t left, const vec4_t right,
    qboolean *flipped)
{
    if (fabsl((long double)right[0] -
              (long double)left[0]) <
            (long double)CM_PATCH_PLANE_NORMAL_EPSILON &&
        fabsl((long double)right[1] -
              (long double)left[1]) <
            (long double)CM_PATCH_PLANE_NORMAL_EPSILON &&
        fabsl((long double)right[2] -
              (long double)left[2]) <
            (long double)CM_PATCH_PLANE_NORMAL_EPSILON &&
        fabsl((long double)right[3] -
              (long double)left[3]) <
            (long double)CM_PATCH_PLANE_DISTANCE_EPSILON) {
        *flipped = qfalse;
        return qtrue;
    }

    if (fabsl((long double)right[0] +
              (long double)left[0]) <
            (long double)CM_PATCH_PLANE_NORMAL_EPSILON &&
        fabsl((long double)right[1] +
              (long double)left[1]) <
            (long double)CM_PATCH_PLANE_NORMAL_EPSILON &&
        fabsl((long double)right[2] +
              (long double)left[2]) <
            (long double)CM_PATCH_PLANE_NORMAL_EPSILON &&
        fabsl((long double)right[3] +
              (long double)left[3]) <
            (long double)CM_PATCH_PLANE_DISTANCE_EPSILON) {
        *flipped = qtrue;
        return qtrue;
    }

    return qfalse;
}

/* Source: CoDUOMP.exe 0x0041e400..0x0041e45f.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041e400_0041e45f.mcode.
 * Name: exact same-module Mac symbol CM_SnapVector. Only the first component
 * within the strict double 0.0001 tolerance is used. */
void CM_SnapVector(vec3_t normal)
{
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        if (fabsl((long double)normal[axis] - 1.0L) <
            (long double)CM_PATCH_PLANE_NORMAL_EPSILON) {
            normal[0] = 0.0f;
            normal[1] = 0.0f;
            normal[2] = 0.0f;
            normal[axis] = 1.0f;
            return;
        }

        if (fabsl((long double)normal[axis] + 1.0L) <
            (long double)CM_PATCH_PLANE_NORMAL_EPSILON) {
            normal[0] = 0.0f;
            normal[1] = 0.0f;
            normal[2] = 0.0f;
            normal[axis] = -1.0f;
            return;
        }
    }
}

/* Source: CoDUOMP.exe 0x0041e460..0x0041e535.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041e460_0041e535.mcode.
 * Name: exact same-module Mac symbol CM_FindPlane. */
int32_t CM_FindPlane(
    const vec4_t plane, qboolean *flipped)
{
    for (int32_t planeIndex = 0;
         planeIndex < cm_patchPlaneCount;
         ++planeIndex) {
        const patchPlane_t *const stored =
            &cm_patchPlanes[planeIndex];
        const vec4_t storedPlane = {
            stored->normal[0],
            stored->normal[1],
            stored->normal[2],
            stored->dist
        };

        if (CM_PlaneEqual(plane, storedPlane,
                          flipped) != qfalse) {
            return planeIndex;
        }
    }

    if (cm_patchPlaneCount ==
        CM_PATCH_PLANE_LIMIT) {
        Com_Error(ERR_DROP,
                  "\x15" "MAX_PATCH_PLANES");
    }

    const int32_t planeIndex =
        cm_patchPlaneCount;
    patchPlane_t *const stored =
        &cm_patchPlanes[planeIndex];
    stored->normal[0] = plane[0];
    stored->normal[1] = plane[1];
    stored->normal[2] = plane[2];
    stored->dist = plane[3];
    stored->signbits = 0;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        if (plane[axis] < 0.0f)
            stored->signbits |=
                (uint32_t)(1U << axis);
    }

    *flipped = qfalse;
    cm_patchPlaneCount++;
    return planeIndex;
}

/* Source: CoDUOMP.exe 0x0041da80..0x0041db38.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041da80_0041db38.mcode.
 * Name: exact same-module Mac symbol CM_PlaneFromPoints. This collision-local
 * variant is instruction-identical to the engine's later PlaneFromPoints
 * body, but writes the plane into a plain vec4_t. Each edge and cross-product
 * component is rounded to float at its original stack/output store. */
qboolean CM_PlaneFromPoints(
    vec4_t plane, const vec3_t point0,
    const vec3_t point1, const vec3_t point2)
{
    vec3_t direction1;
    vec3_t direction2;

    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        direction1[axis] = (float)(
            (long double)point1[axis] -
            (long double)point0[axis]);
        direction2[axis] = (float)(
            (long double)point2[axis] -
            (long double)point0[axis]);
    }

    plane[0] = (float)(
        (long double)direction2[1] *
            (long double)direction1[2] -
        (long double)direction2[2] *
            (long double)direction1[1]);
    plane[1] = (float)(
        (long double)direction2[2] *
            (long double)direction1[0] -
        (long double)direction2[0] *
            (long double)direction1[2]);
    plane[2] = (float)(
        (long double)direction2[0] *
            (long double)direction1[1] -
        (long double)direction2[1] *
            (long double)direction1[0]);

    if (VectorNormalize(plane) == 0.0f)
        return qfalse;

    plane[3] = (float)(
        ((long double)plane[0] *
             (long double)point0[0] +
         (long double)plane[1] *
             (long double)point0[1]) +
        (long double)plane[2] *
            (long double)point0[2]);
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0041db40..0x0041db97.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041db40_0041db98.mcode.
 * Name and signature: exact same-module Mac symbol CM_NeedsSubdivision.
 * MSVC also inlines this body into CM_SubdivideGridColumns. */
qboolean CM_NeedsSubdivision(
    const vec3_t point0, const vec3_t point1,
    const vec3_t point2, int32_t maxError)
{
    long double bend[3];

    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        bend[axis] =
            ((long double)point0[axis] +
             (long double)point2[axis]) -
            ((long double)point1[axis] +
             (long double)point1[axis]);
    }

    const long double lengthSquared =
        (bend[2] * bend[2] +
         bend[1] * bend[1]) +
        bend[0] * bend[0];
    return sqrtl(lengthSquared) * 0.25L >
                   (long double)maxError
               ? qtrue
               : qfalse;
}

/* Source: CoDUOMP.exe 0x0041dba0..0x0041dc20.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041dba0_0041dc21.mcode.
 * Name and signature: exact same-module Mac symbol CM_Subdivide. The rounded
 * first midpoint is reloaded for the center while the second midpoint remains
 * in x87 extended precision after its float store. MSVC also inlines this body
 * into CM_SubdivideGridColumns. */
void CM_Subdivide(
    const vec3_t point0, const vec3_t point1,
    const vec3_t point2, vec3_t midpoint01,
    vec3_t center, vec3_t midpoint12)
{
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        midpoint01[axis] = (float)(
            ((long double)point0[axis] +
             (long double)point1[axis]) *
            0.5L);
        const long double midpoint12Extended =
            ((long double)point1[axis] +
             (long double)point2[axis]) *
            0.5L;
        midpoint12[axis] =
            (float)midpoint12Extended;
        center[axis] = (float)(
            ((long double)midpoint01[axis] +
             midpoint12Extended) *
            0.5L);
    }
}

/* Source: CoDUOMP.exe 0x0041e540..0x0041e6f6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041e540_0041e6f6.mcode.
 * Name: exact same-module Mac symbol CM_FindPlane2. Candidate planes must face
 * the same direction and contain all three input points within the original
 * double 0.1 tolerance. The x87 expressions group Y+Z before X and remain
 * extended through each comparison; the source preserves that ordering. */
int32_t CM_FindPlane2(
    const vec3_t point0, const vec3_t point1,
    const vec3_t point2)
{
    vec4_t plane;
    if (CM_PlaneFromPoints(plane, point0,
                           point1, point2) == qfalse) {
        return -1;
    }

    const float *const points[3] = {
        point0, point1, point2
    };
    for (int32_t planeIndex = 0;
         planeIndex < cm_patchPlaneCount;
        ++planeIndex) {
        const patchPlane_t *const stored =
            &cm_patchPlanes[planeIndex];
        const long double facing =
            ((long double)plane[1] *
                 (long double)stored->normal[1] +
             (long double)plane[2] *
                 (long double)stored->normal[2]) +
            (long double)plane[0] *
                (long double)stored->normal[0];
        if (facing < 0.0L)
            continue;

        int32_t pointIndex;
        for (pointIndex = 0;
             pointIndex < 3;
             ++pointIndex) {
            const float *const point =
                points[pointIndex];
            const long double distance =
                ((long double)point[2] *
                     (long double)stored->normal[2] +
                 (long double)point[1] *
                     (long double)stored->normal[1]) +
                (long double)point[0] *
                    (long double)stored->normal[0] -
                (long double)stored->dist;

            if (distance <
                    -(long double)
                        CM_PATCH_POINT_PLANE_EPSILON ||
                (long double)
                        CM_PATCH_POINT_PLANE_EPSILON <
                    distance) {
                break;
            }
        }

        if (pointIndex == 3)
            return planeIndex;
    }

    if (cm_patchPlaneCount ==
        CM_PATCH_PLANE_LIMIT) {
        Com_Error(ERR_DROP,
                  "\x15" "MAX_PATCH_PLANES");
    }

    const int32_t planeIndex =
        cm_patchPlaneCount;
    patchPlane_t *const stored =
        &cm_patchPlanes[planeIndex];
    stored->normal[0] = plane[0];
    stored->normal[1] = plane[1];
    stored->normal[2] = plane[2];
    stored->dist = plane[3];
    stored->signbits = 0;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        if (plane[axis] < 0.0f)
            stored->signbits |=
                (uint32_t)(1U << axis);
    }

    cm_patchPlaneCount++;
    return planeIndex;
}

/* Source: CoDUOMP.exe 0x0041e700..0x0041e754, recovered from the executable
 * gap. Name and signature: exact same-module Mac symbol
 * CM_PointOnPlaneSide. The Windows body retains the dot product in x87
 * extended precision and groups the Z and Y terms before adding X. */
int32_t CM_PointOnPlaneSide(const vec3_t point, int32_t planeIndex)
{
    if (planeIndex == -1)
        return CM_WINDING_SIDE_ON;

    const patchPlane_t *const plane =
        &cm_patchPlanes[planeIndex];
    const long double distance =
        ((long double)point[2] *
             (long double)plane->normal[2] +
         (long double)point[1] *
             (long double)plane->normal[1]) +
        (long double)point[0] *
            (long double)plane->normal[0] -
        (long double)plane->dist;

    if ((long double)CM_PATCH_POINT_PLANE_EPSILON <
        distance) {
        return CM_WINDING_SIDE_FRONT;
    }
    if (distance <
        -(long double)CM_PATCH_POINT_PLANE_EPSILON) {
        return CM_WINDING_SIDE_BACK;
    }
    return CM_WINDING_SIDE_ON;
}

/* Source: CoDUOMP.exe 0x0041e760..0x0041e78b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041e760_0041e78c.mcode.
 * Name and signature: exact same-module Mac symbol CM_GridPlane. If the
 * requested triangle has no plane, the other triangle in the same grid cell
 * supplies the fallback value. */
int32_t CM_GridPlane(const patchPlaneGrid_t planeGrid,
                     int32_t x, int32_t y, int32_t triangle)
{
    const int32_t plane = planeGrid[x][y][triangle];
    if (plane != -1)
        return plane;
    return planeGrid[x][y][triangle == 0 ? 1 : 0];
}

/* Source: CoDUOMP.exe 0x0041e790..0x0041e9f5.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041e790_0041e9f5.mcode.
 * Name: exact same-module Mac symbol CM_EdgePlaneNum. The adjacent surface
 * plane supplies a point pushed four units along its normal; that third point
 * and the selected edge define the border plane. */
int32_t CM_EdgePlaneNum(
    const cGrid_t *grid,
    const patchPlaneGrid_t planeGrid,
    int32_t x, int32_t y, int32_t edge)
{
    const float *point0;
    const float *point1;
    int32_t planeSlot;
    qboolean reversePoints;

    switch (edge) {
    case CM_PATCH_EDGE_BOTTOM:
        point0 = grid->points[x][y];
        point1 = grid->points[x + 1][y];
        planeSlot = 0;
        reversePoints = qfalse;
        break;
    case CM_PATCH_EDGE_RIGHT:
        point0 = grid->points[x + 1][y];
        point1 = grid->points[x + 1][y + 1];
        planeSlot = 0;
        reversePoints = qfalse;
        break;
    case CM_PATCH_EDGE_TOP:
        point0 = grid->points[x][y + 1];
        point1 = grid->points[x + 1][y + 1];
        planeSlot = 1;
        reversePoints = qtrue;
        break;
    case CM_PATCH_EDGE_LEFT:
        point0 = grid->points[x][y];
        point1 = grid->points[x][y + 1];
        planeSlot = 1;
        reversePoints = qtrue;
        break;
    case CM_PATCH_EDGE_DIAGONAL_DESCENDING:
        point0 = grid->points[x + 1][y + 1];
        point1 = grid->points[x][y];
        planeSlot = 0;
        reversePoints = qfalse;
        break;
    case CM_PATCH_EDGE_DIAGONAL_ASCENDING:
        point0 = grid->points[x][y];
        point1 = grid->points[x + 1][y + 1];
        planeSlot = 1;
        reversePoints = qfalse;
        break;
    default:
        Com_Error(ERR_DROP,
                  "\x15"
                  "CM_EdgePlaneNum: bad edge name");
        return -1;
    }

    int32_t adjacentPlane =
        planeGrid[x][y][planeSlot];
    if (adjacentPlane == -1) {
        adjacentPlane =
            planeGrid[x][y][planeSlot == 0 ? 1 : 0];
    }
    if (adjacentPlane < 0)
        return -1;

    vec3_t offsetPoint;
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        offsetPoint[axis] = (float)(
            (long double)
                    cm_patchPlanes[adjacentPlane]
                        .normal[axis] *
                4.0L +
            (long double)point0[axis]);
    }

    if (reversePoints != qfalse) {
        return CM_FindPlane2(point1, point0,
                             offsetPoint);
    }
    return CM_FindPlane2(point0, point1,
                         offsetPoint);
}

/* Source: CoDUOMP.exe 0x0041ea10..0x0041ed42.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041ea10_0041ed42.mcode.
 * Name: exact same-module Mac symbol CM_SetBorderInward. Every border plane is
 * classified against the three triangle vertices or four quad vertices.
 * Distances remain extended and use strict double ±0.1 thresholds. */
void CM_SetBorderInward(
    facet_t *facet,
    const cGrid_t *grid,
    const patchPlaneGrid_t planeGrid,
    int32_t x, int32_t y, int32_t mode)
{
    (void)planeGrid;

    const float *vertices[4];
    int32_t vertexCount;
    if (mode ==
        CM_PATCH_BORDER_INWARD_LOWER_TRIANGLE) {
        vertices[0] = grid->points[x][y];
        vertices[1] = grid->points[x + 1][y];
        vertices[2] =
            grid->points[x + 1][y + 1];
        vertexCount = 3;
    } else if (mode ==
               CM_PATCH_BORDER_INWARD_QUAD) {
        vertices[0] = grid->points[x][y];
        vertices[1] = grid->points[x + 1][y];
        vertices[2] =
            grid->points[x + 1][y + 1];
        vertices[3] = grid->points[x][y + 1];
        vertexCount = 4;
    } else if (mode ==
               CM_PATCH_BORDER_INWARD_UPPER_TRIANGLE) {
        vertices[0] =
            grid->points[x + 1][y + 1];
        vertices[1] = grid->points[x][y + 1];
        vertices[2] = grid->points[x][y];
        vertexCount = 3;
    } else {
        Com_Error(0,
                  "\x15"
                  "CM_SetBorderInward: bad parameter");
        vertexCount = 0;
    }

    for (int32_t borderIndex = 0;
         borderIndex < facet->numBorders;
         ++borderIndex) {
        int32_t frontCount = 0;
        int32_t backCount = 0;
        const int32_t planeIndex =
            facet->borderPlanes[borderIndex];

        if (planeIndex != -1) {
            for (int32_t vertexIndex = 0;
                 vertexIndex < vertexCount;
                 ++vertexIndex) {
                const float *const point =
                    vertices[vertexIndex];
                const int32_t side =
                    CM_PointOnPlaneSide(point, planeIndex);

                if (side == CM_WINDING_SIDE_FRONT) {
                    frontCount++;
                } else if (side == CM_WINDING_SIDE_BACK) {
                    backCount++;
                }
            }
        }

        if (frontCount != 0) {
            if (backCount == 0) {
                facet->borderInward[borderIndex] =
                    qtrue;
            } else {
                Com_DPrintf(
                    "WARNING: CM_SetBorderInward: "
                    "mixed plane sides\n");
                facet->borderInward[borderIndex] =
                    qfalse;
            }
        } else if (backCount != 0) {
            facet->borderInward[borderIndex] =
                qfalse;
        } else {
            facet->borderPlanes[borderIndex] = -1;
        }
    }
}

/* Source: CoDUOMP.exe 0x0041ed50..0x0041ef37.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041ed50_0041ef37.mcode.
 * Name: exact same-module Mac symbol CM_ValidateFacet. Clipping must leave a
 * nonempty winding whose bounds stay inside the original ±131072 world
 * coordinate domain and whose extent does not exceed 131072 on any axis. */
qboolean CM_ValidateFacet(
    const facet_t *facet)
{
    if (facet->surfacePlane == -1)
        return qfalse;

    vec4_t plane;
    CM_LoadPatchFacetPlane(
        facet->surfacePlane, qtrue, plane);
    winding_t *winding =
        BaseWindingForPlane(plane, plane[3]);

    for (int32_t borderIndex = 0;
         borderIndex < facet->numBorders &&
         winding != NULL;
         ++borderIndex) {
        if (facet->borderPlanes[borderIndex] ==
            -1) {
            FreeWinding(winding);
            return qfalse;
        }

        CM_LoadPatchFacetPlane(
            facet->borderPlanes[borderIndex],
            facet->borderInward[borderIndex],
            plane);
        ChopWindingInPlace(
            &winding, plane, plane[3],
            CM_PATCH_CHOP_EPSILON);
    }

    if (winding == NULL)
        return qfalse;

    vec3_t mins;
    vec3_t maxs;
    WindingBounds(winding, mins, maxs);
    FreeWinding(winding);

    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        if ((long double)maxs[axis] -
                    (long double)mins[axis] >
                (long double)
                    CM_PATCH_WORLD_COORD_LIMIT ||
            mins[axis] >=
                (float)CM_PATCH_WORLD_COORD_LIMIT ||
            maxs[axis] <=
                (float)-CM_PATCH_WORLD_COORD_LIMIT) {
            return qfalse;
        }
    }

    return qtrue;
}

/* Source: CoDUOMP.exe 0x0041ef40..0x0041f7a3.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041ef40_0041f7a3.mcode.
 * Name: exact same-module Mac symbol CM_AddFacetBevels. */
void CM_AddFacetBevels(facet_t *facet)
{
    vec4_t surfacePlane;
    CM_LoadPatchFacetPlane(facet->surfacePlane, qtrue, surfacePlane);
    winding_t *winding =
        BaseWindingForPlane(surfacePlane, surfacePlane[3]);

    for (int32_t borderIndex = 0;
         borderIndex < facet->numBorders && winding != NULL;
         ++borderIndex) {
        if (facet->borderPlanes[borderIndex] == facet->surfacePlane)
            continue;

        vec4_t plane;
        CM_LoadPatchFacetPlane(facet->borderPlanes[borderIndex],
                               facet->borderInward[borderIndex], plane);
        ChopWindingInPlace(&winding, plane, plane[3],
                           CM_PATCH_CHOP_EPSILON);
    }

    if (winding == NULL)
        return;

    vec3_t mins;
    vec3_t maxs;
    WindingBounds(winding, mins, maxs);

    for (int32_t axis = 0; axis < CM_PATCH_VECTOR_AXIS_COUNT; ++axis) {
        for (int32_t direction = CM_PATCH_BEVEL_AXIS_MIN;
             direction <= CM_PATCH_BEVEL_AXIS_MAX; direction += 2) {
            vec4_t bevel = {0.0f, 0.0f, 0.0f, 0.0f};
            bevel[axis] = (float)direction;
            bevel[3] = direction == 1 ? maxs[axis] : -mins[axis];

            qboolean flipped;
            if (CM_PlaneEqual(surfacePlane, bevel, &flipped) != qfalse)
                continue;

            int32_t borderIndex;
            for (borderIndex = 0;
                 borderIndex < facet->numBorders;
                 ++borderIndex) {
                vec4_t border;
                CM_LoadPatchFacetPlane(facet->borderPlanes[borderIndex],
                                       qtrue, border);
                if (CM_PlaneEqual(border, bevel, &flipped) != qfalse)
                    break;
            }

            if (borderIndex != facet->numBorders)
                continue;

            /* NOT_FROM_ORIGINAL_SOURCE: reserve one border slot for the
             * mandatory surface-plane entry appended below. */
            if (facet->numBorders >= CM_PATCH_FACET_BEVEL_BORDER_LIMIT) {
                Com_Error(ERR_DROP, "\x15" "CM_AddFacetBevels: too many bevels");
            }

            const int32_t insert = facet->numBorders;
            facet->borderPlanes[insert] =
                CM_FindPlane(bevel, &flipped);
            facet->borderNoAdjust[insert] = qfalse;
            facet->borderInward[insert] = flipped;
            facet->numBorders++;
        }
    }

    for (int32_t pointIndex = 0;
         pointIndex < winding->numpoints;
         ++pointIndex) {
        const int32_t nextIndex =
            (pointIndex + 1) % winding->numpoints;
        vec3_t edge;
        for (int32_t axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT;
             ++axis) {
            edge[axis] = (float)(
                (long double)winding->p[pointIndex][axis] -
                (long double)winding->p[nextIndex][axis]);
        }

        if ((double)VectorNormalize(edge) <
            CM_PATCH_BEVEL_EDGE_LENGTH_MIN) {
            continue;
        }

        CM_SnapVector(edge);

        int32_t axis;
        for (axis = 0;
             axis < CM_PATCH_VECTOR_AXIS_COUNT &&
             edge[axis] != -1.0f && edge[axis] != 1.0f;
             ++axis) {
        }
        if (axis < CM_PATCH_VECTOR_AXIS_COUNT)
            continue;

        for (axis = 0; axis < CM_PATCH_VECTOR_AXIS_COUNT; ++axis) {
            for (int32_t direction = CM_PATCH_BEVEL_AXIS_MIN;
                 direction <= CM_PATCH_BEVEL_AXIS_MAX;
                 direction += 2) {
                vec3_t axial = {0.0f, 0.0f, 0.0f};
                axial[axis] = (float)direction;

                vec4_t bevel;
                CrossProduct(edge, axial, bevel);
                if ((double)VectorNormalize(bevel) <
                    CM_PATCH_BEVEL_EDGE_LENGTH_MIN) {
                    continue;
                }

                /* Windows forms x + (y + z); Linux forms (x + y) + z.  Both
                 * round only the final value to the float bevel-distance
                 * slot. */
                bevel[3] = (float)(
                    (long double)winding->p[pointIndex][0] *
                        (long double)bevel[0] +
                    ((long double)winding->p[pointIndex][1] *
                         (long double)bevel[1] +
                     (long double)winding->p[pointIndex][2] *
                         (long double)bevel[2]));

                qboolean hasBackPoint = qfalse;
                int32_t checkPoint;
                for (checkPoint = 0;
                     checkPoint < winding->numpoints;
                     ++checkPoint) {
                    const float dist = (float)(
                        (long double)winding->p[checkPoint][0] *
                            (long double)bevel[0] +
                        ((long double)winding->p[checkPoint][1] *
                             (long double)bevel[1] +
                         (long double)winding->p[checkPoint][2] *
                             (long double)bevel[2]) -
                         (long double)bevel[3]);

                    if ((double)dist >
                        CM_PATCH_BEVEL_PLANE_EPSILON) {
                        break;
                    }
                    if ((double)dist <
                        -CM_PATCH_BEVEL_PLANE_EPSILON) {
                        hasBackPoint = qtrue;
                    }
                }

                if (checkPoint != winding->numpoints ||
                    hasBackPoint == qfalse) {
                    continue;
                }

                int32_t borderIndex;
                qboolean flipped;
                for (borderIndex = 0;
                     borderIndex < facet->numBorders;
                     ++borderIndex) {
                    vec4_t border;
                    CM_LoadPatchFacetPlane(
                        facet->borderPlanes[borderIndex],
                        qtrue, border);
                    if (CM_PlaneEqual(border, bevel,
                                      &flipped) != qfalse) {
                        break;
                    }
                }

                if (borderIndex != facet->numBorders)
                    continue;

                /* NOT_FROM_ORIGINAL_SOURCE: a provisional edge may use the
                 * final slot only until its candidate result is known. */
                if (facet->numBorders >= CM_PATCH_FACET_BORDER_PLANE_LIMIT) {
                    Com_Error(ERR_DROP, "\x15" "CM_AddFacetBevels: too many borders");
                }

                const int32_t insert = facet->numBorders;
                facet->borderPlanes[insert] =
                    CM_FindPlane(bevel, &flipped);
                for (int32_t prior = 0;
                     prior < facet->numBorders;
                     ++prior) {
                    if (facet->borderPlanes[insert] ==
                        facet->borderPlanes[prior]) {
                        Com_Printf(
                            "WARNING: bevel plane already used\n");
                    }
                }

                facet->borderNoAdjust[insert] = qfalse;
                facet->borderInward[insert] = flipped;

                winding_t *test = CopyWinding(winding);
                vec4_t plane;
                CM_LoadPatchFacetPlane(
                    facet->borderPlanes[insert],
                    facet->borderInward[insert], plane);
                ChopWindingInPlace(&test, plane, plane[3],
                                   CM_PATCH_CHOP_EPSILON);
                if (test == NULL) {
                    Com_DPrintf(
                        "WARNING: CM_AddFacetBevels... invalid bevel\n");
                } else {
                    FreeWinding(test);
                    /* NOT_FROM_ORIGINAL_SOURCE: once accepted, the candidate
                     * must still leave room for the mandatory surface plane. */
                    if (facet->numBorders >= CM_PATCH_FACET_BEVEL_BORDER_LIMIT) {
                        Com_Error(ERR_DROP, "\x15" "CM_AddFacetBevels: too many bevels");
                    }
                    facet->numBorders++;
                }
            }
        }
    }

    FreeWinding(winding);

    /* NOT_FROM_ORIGINAL_SOURCE: retain a final capacity gate immediately
     * before the mandatory append. */
    if (facet->numBorders >= CM_PATCH_FACET_BORDER_PLANE_LIMIT) {
        Com_Error(ERR_DROP, "\x15" "CM_AddFacetBevels: too many borders");
    }
    const int32_t insert = facet->numBorders;
    facet->borderPlanes[insert] = facet->surfacePlane;
    facet->borderNoAdjust[insert] = qfalse;
    facet->borderInward[insert] = qtrue;
    facet->numBorders++;
}

/* Source: CoDUOMP.exe 0x0041f7b0..0x0041ffb6.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041f7b0_0041ffb6.mcode.
 * Name: exact same-module Mac symbol CM_PatchCollideFromGrid.
 *
 * The 0x708a8-byte probed Windows stack frame contains planeGrid and facets.
 * Keeping both arrays local preserves the client executable's ownership; they
 * are copied into permanent hunk storage before the function returns. */
void CM_PatchCollideFromGrid(const cGrid_t *grid,
                             patchCollide_t *patchCollide)
{
    int32_t (*mutablePlaneGrid)[CM_PATCH_PLANE_GRID_SIZE][2];
    facet_t *facets;
    patchPlaneGrid_t localPlaneGrid;
    facet_t localFacets[CM_PATCH_MAX_FACETS];
    mutablePlaneGrid = localPlaneGrid;
    facets = localFacets;
    int32_t numFacets = 0;

    cm_patchPlaneCount = 0;

    for (int32_t x = 0; x < grid->width - 1; ++x) {
        for (int32_t y = 0; y < grid->height - 1; ++y) {
            mutablePlaneGrid[x][y][0] =
                CM_FindPlane2(grid->points[x][y],
                              grid->points[x + 1][y],
                              grid->points[x + 1][y + 1]);
            mutablePlaneGrid[x][y][1] =
                CM_FindPlane2(grid->points[x + 1][y + 1],
                              grid->points[x][y + 1],
                              grid->points[x][y]);
        }
    }

    /* ISO C before C23 does not propagate const through an array typedef.
     * Use one explicit read-only view after construction. */
    const int32_t (*const planeGrid)[CM_PATCH_PLANE_GRID_SIZE][2] =
        (const int32_t (*)[CM_PATCH_PLANE_GRID_SIZE][2])mutablePlaneGrid;

    for (int32_t x = 0; x < grid->width - 1; ++x) {
        for (int32_t y = 0; y < grid->height - 1; ++y) {
            int32_t bottomPlane = -1;
            if (y == 0) {
                if (grid->wrapHeight != qfalse) {
                    bottomPlane =
                        planeGrid[x][grid->height - 2][1];
                }
            } else {
                bottomPlane = planeGrid[x][y - 1][1];
            }
            const qboolean bottomHasCoplanarNeighbor =
                bottomPlane == planeGrid[x][y][0]
                    ? qtrue
                    : qfalse;
            if (bottomPlane == -1 ||
                bottomHasCoplanarNeighbor != qfalse) {
                bottomPlane =
                    CM_EdgePlaneNum(grid, planeGrid, x, y,
                                    CM_PATCH_EDGE_BOTTOM);
            }

            int32_t topPlane = -1;
            if (y < grid->height - 2) {
                topPlane = planeGrid[x][y + 1][0];
            } else if (grid->wrapHeight != qfalse) {
                topPlane = planeGrid[x][0][0];
            }
            const qboolean topHasCoplanarNeighbor =
                topPlane == planeGrid[x][y][1]
                    ? qtrue
                    : qfalse;
            if (topPlane == -1 ||
                topHasCoplanarNeighbor != qfalse) {
                topPlane =
                    CM_EdgePlaneNum(grid, planeGrid, x, y,
                                    CM_PATCH_EDGE_TOP);
            }

            int32_t leftPlane = -1;
            if (x == 0) {
                if (grid->wrapWidth != qfalse) {
                    leftPlane =
                        planeGrid[grid->width - 2][y][0];
                }
            } else {
                leftPlane = planeGrid[x - 1][y][0];
            }
            const qboolean leftHasCoplanarNeighbor =
                leftPlane == planeGrid[x][y][1]
                    ? qtrue
                    : qfalse;
            if (leftPlane == -1 ||
                leftHasCoplanarNeighbor != qfalse) {
                leftPlane =
                    CM_EdgePlaneNum(grid, planeGrid, x, y,
                                    CM_PATCH_EDGE_LEFT);
            }

            int32_t rightPlane = -1;
            if (x < grid->width - 2) {
                rightPlane = planeGrid[x + 1][y][1];
            } else if (grid->wrapWidth != qfalse) {
                rightPlane = planeGrid[0][y][1];
            }
            const qboolean rightHasCoplanarNeighbor =
                rightPlane == planeGrid[x][y][0]
                    ? qtrue
                    : qfalse;
            if (rightPlane == -1 ||
                rightHasCoplanarNeighbor != qfalse) {
                rightPlane =
                    CM_EdgePlaneNum(grid, planeGrid, x, y,
                                    CM_PATCH_EDGE_RIGHT);
            }

            if (numFacets == CM_PATCH_MAX_FACETS) {
                Com_Error(ERR_DROP,
                          "\x15" "MAX_FACETS");
            }

            facet_t *facet = &facets[numFacets];
            Com_Memset(facet, 0, sizeof(*facet));

            if (planeGrid[x][y][0] == planeGrid[x][y][1]) {
                if (planeGrid[x][y][0] != -1) {
                    facet->surfacePlane = planeGrid[x][y][0];
                    facet->numBorders = 4;
                    facet->borderPlanes[0] = bottomPlane;
                    facet->borderNoAdjust[0] =
                        bottomHasCoplanarNeighbor;
                    facet->borderPlanes[1] = rightPlane;
                    facet->borderNoAdjust[1] =
                        rightHasCoplanarNeighbor;
                    facet->borderPlanes[2] = topPlane;
                    facet->borderNoAdjust[2] =
                        topHasCoplanarNeighbor;
                    facet->borderPlanes[3] = leftPlane;
                    facet->borderNoAdjust[3] =
                        leftHasCoplanarNeighbor;
                    CM_SetBorderInward(
                        facet, grid, planeGrid, x, y,
                        CM_PATCH_BORDER_INWARD_QUAD);
                    if (CM_ValidateFacet(facet) != qfalse) {
                        CM_AddFacetBevels(facet);
                        numFacets++;
                    }
                }
                continue;
            }

            facet->surfacePlane = planeGrid[x][y][0];
            facet->numBorders = 3;
            facet->borderPlanes[0] = bottomPlane;
            facet->borderNoAdjust[0] =
                bottomHasCoplanarNeighbor;
            facet->borderPlanes[1] = rightPlane;
            facet->borderNoAdjust[1] =
                rightHasCoplanarNeighbor;
            facet->borderPlanes[2] = planeGrid[x][y][1];
            if (facet->borderPlanes[2] == -1) {
                facet->borderPlanes[2] =
                    topPlane != -1
                        ? topPlane
                        : CM_EdgePlaneNum(
                              grid, planeGrid, x, y,
                              CM_PATCH_EDGE_DIAGONAL_DESCENDING);
            }
            CM_SetBorderInward(
                facet, grid, planeGrid, x, y,
                CM_PATCH_BORDER_INWARD_LOWER_TRIANGLE);
            if (CM_ValidateFacet(facet) != qfalse) {
                CM_AddFacetBevels(facet);
                numFacets++;
            }

            if (numFacets == CM_PATCH_MAX_FACETS) {
                Com_Error(ERR_DROP,
                          "\x15" "MAX_FACETS");
            }

            facet = &facets[numFacets];
            Com_Memset(facet, 0, sizeof(*facet));
            facet->surfacePlane = planeGrid[x][y][1];
            facet->numBorders = 3;
            facet->borderPlanes[0] = topPlane;
            facet->borderNoAdjust[0] =
                topHasCoplanarNeighbor;
            facet->borderPlanes[1] = leftPlane;
            facet->borderNoAdjust[1] =
                leftHasCoplanarNeighbor;
            facet->borderPlanes[2] = planeGrid[x][y][0];
            if (facet->borderPlanes[2] == -1) {
                facet->borderPlanes[2] =
                    bottomPlane != -1
                        ? bottomPlane
                        : CM_EdgePlaneNum(
                              grid, planeGrid, x, y,
                              CM_PATCH_EDGE_DIAGONAL_ASCENDING);
            }
            CM_SetBorderInward(
                facet, grid, planeGrid, x, y,
                CM_PATCH_BORDER_INWARD_UPPER_TRIANGLE);
            if (CM_ValidateFacet(facet) != qfalse) {
                CM_AddFacetBevels(facet);
                numFacets++;
            }
        }
    }

    patchCollide->numPlanes = cm_patchPlaneCount;
    patchCollide->numFacets = numFacets;
    patchCollide->facets =
        Hunk_AllocAlignInternal(
        (size_t)numFacets * sizeof(patchCollide->facets[0]),
        CM_PATCH_HUNK_ALIGNMENT);
    Com_Memcpy(
        patchCollide->facets, facets,
        (size_t)numFacets * sizeof(patchCollide->facets[0]));
    patchCollide->planes =
        Hunk_AllocAlignInternal(
        (size_t)cm_patchPlaneCount *
            sizeof(patchCollide->planes[0]),
        CM_PATCH_HUNK_ALIGNMENT);
    Com_Memcpy(
        patchCollide->planes, cm_patchPlanes,
        (size_t)cm_patchPlaneCount *
            sizeof(patchCollide->planes[0]));
}

/* Source: CoDUOMP.exe 0x0041dc30..0x0041dd82.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041dc30_0041dd83.mcode.
 * Name and grid interface: exact same-module Mac symbol CM_TransposeGrid.
 * The two rectangular-grid paths preserve the original one-sided copies
 * outside the square overlap before swapping dimensions and wrap flags. */
void CM_TransposeGrid(cGrid_t *grid)
{
    if (grid->height < grid->width) {
        for (int32_t x = 0; x < grid->height; ++x) {
            for (int32_t y = x + 1; y < grid->width; ++y) {
                if (y < grid->height) {
                    vec3_t temporary;

                    for (int32_t axis = 0; axis < 3; ++axis) {
                        temporary[axis] =
                            grid->points[x][y][axis];
                        grid->points[x][y][axis] =
                            grid->points[y][x][axis];
                        grid->points[y][x][axis] =
                            temporary[axis];
                    }
                } else {
                    for (int32_t axis = 0; axis < 3; ++axis) {
                        grid->points[x][y][axis] =
                            grid->points[y][x][axis];
                    }
                }
            }
        }
    } else {
        for (int32_t x = 0; x < grid->width; ++x) {
            for (int32_t y = x + 1; y < grid->height; ++y) {
                if (y < grid->width) {
                    vec3_t temporary;

                    for (int32_t axis = 0; axis < 3; ++axis) {
                        temporary[axis] =
                            grid->points[y][x][axis];
                        grid->points[y][x][axis] =
                            grid->points[x][y][axis];
                        grid->points[x][y][axis] =
                            temporary[axis];
                    }
                } else {
                    for (int32_t axis = 0; axis < 3; ++axis) {
                        grid->points[y][x][axis] =
                            grid->points[x][y][axis];
                    }
                }
            }
        }
    }

    {
        const int32_t width = grid->width;
        const qboolean wrapWidth = grid->wrapWidth;

        grid->width = grid->height;
        grid->height = width;
        grid->wrapWidth = grid->wrapHeight;
        grid->wrapHeight = wrapWidth;
    }
}

/* Source: CoDUOMP.exe 0x0041dd90..0x0041de24.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041dd90_0041de25.mcode.
 * Name and grid interface: exact same-module Mac symbol
 * CM_SetGridWrapWidth. The ordered long-double comparisons preserve the
 * original x87 subtraction against the exact double +/-0.1 constants. */
void CM_SetGridWrapWidth(cGrid_t *grid)
{
    int32_t y;

    for (y = 0; y < grid->height; ++y) {
        int32_t axis;

        for (axis = 0; axis < 3; ++axis) {
            const long double delta =
                (long double)grid->points[0][y][axis] -
                (long double)
                    grid->points[grid->width - 1][y][axis];

            if (delta <
                    -(long double)CM_PATCH_GRID_POINT_EPSILON ||
                delta >
                    (long double)CM_PATCH_GRID_POINT_EPSILON) {
                break;
            }
        }
        if (axis != 3) {
            break;
        }
    }

    grid->wrapWidth =
        y == grid->height ? qtrue : qfalse;
}

/* Source: CoDUOMP.exe 0x0041de30..0x0041e165.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041de30_0041e166.mcode.
 * Name and max-error interface: exact same-module Mac symbol
 * CM_SubdivideGridColumns. Windows proves the x87 bend-length test,
 * two-column insertion shift, and float-rounded three-point subdivision. */
void CM_SubdivideGridColumns(cGrid_t *grid, int32_t maxError)
{
    int32_t x = 0;
    const float maxErrorAsFloat = (float)maxError;

    while (x < grid->width - 2) {
        int32_t y;

        for (y = 0; y < grid->height; ++y) {
            long double bend[3];

            for (int32_t axis = 0; axis < 3; ++axis) {
                bend[axis] =
                    ((long double)grid->points[x][y][axis] +
                     (long double)grid->points[x + 2][y][axis]) -
                    ((long double)grid->points[x + 1][y][axis] +
                     (long double)grid->points[x + 1][y][axis]);
            }
            /* The x87 stack sums y^2 + z^2 first, then adds x^2. */
            const long double lengthSquared =
                bend[0] * bend[0] +
                (bend[1] * bend[1] +
                 bend[2] * bend[2]);
            /* 0x0041de61..0x0041de6b converts the integer with FILD and
             * stores it to a float local before the later x87 comparison. */
            if ((long double)maxErrorAsFloat <
                sqrtl(lengthSquared) * 0.25L) {
                break;
            }
        }

        if (y == grid->height) {
            x += 2;
            continue;
        }

        /* NOT_FROM_ORIGINAL_SOURCE: reserve both generated grid columns before
         * the first shifted or generated point write. */
        if (grid->width > CM_PATCH_POINT_GRID_SIZE - 2) {
            Com_Error(ERR_DROP, "\x15" "CM_SubdivideGridColumns: MAX_GRID_SIZE");
        }

        for (y = 0; y < grid->height; ++y) {
            vec3_t point0;
            vec3_t point1;
            vec3_t point2;

            for (int32_t axis = 0; axis < 3; ++axis) {
                point0[axis] =
                    grid->points[x][y][axis];
                point1[axis] =
                    grid->points[x + 1][y][axis];
                point2[axis] =
                    grid->points[x + 2][y][axis];
            }

            for (int32_t source = grid->width - 1;
                 source > x + 1;
                 --source) {
                for (int32_t axis = 0; axis < 3; ++axis) {
                    grid->points[source + 2][y][axis] =
                        grid->points[source][y][axis];
                }
            }

            for (int32_t axis = 0; axis < 3; ++axis) {
                const float midpoint01 = (float)(
                    ((long double)point0[axis] +
                     (long double)point1[axis]) * 0.5L);
                const long double midpoint12Extended =
                    ((long double)point1[axis] +
                     (long double)point2[axis]) * 0.5L;
                const float midpoint12 =
                    (float)midpoint12Extended;

                grid->points[x + 1][y][axis] =
                    midpoint01;
                grid->points[x + 3][y][axis] =
                    midpoint12;
                /* FST stores midpoint12 to float but retains the extended
                 * value in ST0 for the center calculation. */
                grid->points[x + 2][y][axis] = (float)(
                    ((long double)midpoint01 +
                     midpoint12Extended) * 0.5L);
            }
        }

        grid->width += 2;
    }
}

/* Source: CoDUOMP.exe 0x0041e170..0x0041e1d9, recovered from the executable
 * gap. Name and signature: exact same-module Mac symbol CM_ComparePoints.
 * The retained Windows body performs each float subtraction in x87 extended
 * precision and accepts the inclusive double-precision +/-0.1 interval. */
qboolean CM_ComparePoints(const vec3_t left, const vec3_t right)
{
    for (int32_t axis = 0;
         axis < CM_PATCH_VECTOR_AXIS_COUNT;
         ++axis) {
        const long double delta =
            (long double)left[axis] -
            (long double)right[axis];
        if (delta <
                -(long double)CM_PATCH_GRID_POINT_EPSILON ||
            (long double)CM_PATCH_GRID_POINT_EPSILON <
                delta) {
            return qfalse;
        }
    }
    return qtrue;
}

/* Source: CoDUOMP.exe 0x0041e1e0..0x0041e309.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041e1e0_0041e30a.mcode.
 * Name and grid interface: exact same-module Mac symbol
 * CM_RemoveDegenerateColumns. Column equality retains the original x87
 * subtraction and exact double +/-0.1 comparisons. */
void CM_RemoveDegenerateColumns(cGrid_t *grid)
{
    for (int32_t x = 0;
         x < grid->width - 1;
         ++x) {
        int32_t y;

        for (y = 0; y < grid->height; ++y) {
            int32_t axis;

            for (axis = 0; axis < 3; ++axis) {
                const long double delta =
                    (long double)grid->points[x][y][axis] -
                    (long double)
                        grid->points[x + 1][y][axis];

                if (delta <
                        -(long double)
                            CM_PATCH_GRID_POINT_EPSILON ||
                    delta >
                        (long double)
                            CM_PATCH_GRID_POINT_EPSILON) {
                    break;
                }
            }
            if (axis != 3) {
                break;
            }
        }

        if (y != grid->height) {
            continue;
        }

        for (y = 0; y < grid->height; ++y) {
            for (int32_t source = x + 2;
                 source < grid->width;
                 ++source) {
                for (int32_t axis = 0; axis < 3; ++axis) {
                    grid->points[source - 1][y][axis] =
                        grid->points[source][y][axis];
                }
            }
        }
        --grid->width;
        --x;
    }
}

/* Source: CoDUOMP.exe 0x0041ffc0..0x0042038b.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_0041ffc0_0042038c.mcode.
 * Name and signature: exact same-module Mac symbol
 * CM_GeneratePatchCollide. Windows proves the grid dimensions and parity
 * checks, transposed source-to-grid copy, two subdivision passes, native
 * patch-collide allocation, transformed-grid bounds, and one-unit expansion.
 */
patchCollide_t *CM_GeneratePatchCollide(
    uint32_t width, uint32_t height, int32_t maxError,
    const vec3_t *points, vec3_t bounds[2])
{
    cGrid_t localGrid;
    cGrid_t *const grid = &localGrid;

    if ((int32_t)width <= 2 ||
        (int32_t)height <= 2 ||
        points == NULL) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CM_GeneratePatchFacets: bad parameters: (%i, %i, %p)",
            width, height, (const void *)points);
    }
    if ((width & 1U) == 0 ||
        (height & 1U) == 0) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CM_GeneratePatchFacets: even sizes are invalid for quadratic meshes");
    }
    if ((int32_t)width > CM_PATCH_POINT_GRID_SIZE ||
        (int32_t)height > CM_PATCH_POINT_GRID_SIZE) {
        Com_Error(
            ERR_DROP,
            "\x15"
            "CM_GeneratePatchFacets: source is > MAX_GRID_SIZE");
    }

    grid->width = (int32_t)width;
    grid->height = (int32_t)height;
    grid->wrapWidth = qfalse;
    grid->wrapHeight = qfalse;

    for (int32_t x = 0; x < (int32_t)width; ++x) {
        for (int32_t y = 0; y < (int32_t)height; ++y) {
            for (int32_t axis = 0; axis < 3; ++axis) {
                grid->points[x][y][axis] =
                    points[y * width + (uint32_t)x][axis];
            }
        }
    }

    CM_SetGridWrapWidth(grid);
    CM_SubdivideGridColumns(grid, maxError);
    CM_RemoveDegenerateColumns(grid);
    CM_TransposeGrid(grid);
    CM_SetGridWrapWidth(grid);
    CM_SubdivideGridColumns(grid, maxError);
    CM_RemoveDegenerateColumns(grid);

    {
        patchCollide_t *const patchCollide =
            Hunk_AllocAlignInternal(
                sizeof(*patchCollide),
                CM_PATCH_HUNK_ALIGNMENT);

        ClearBounds(bounds[0], bounds[1]);
        for (int32_t x = 0; x < grid->width; ++x) {
            for (int32_t y = 0; y < grid->height; ++y) {
                AddPointToBounds(
                    grid->points[x][y],
                    bounds[0], bounds[1]);
            }
        }

        CM_PatchCollideFromGrid(grid, patchCollide);

        for (int32_t axis = 0; axis < 3; ++axis) {
            bounds[0][axis] -= 1.0f;
            bounds[1][axis] += 1.0f;
        }
        return patchCollide;
    }
}
