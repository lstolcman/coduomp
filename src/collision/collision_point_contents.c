#include "collision_point_contents.h"

#include "qcommon/collision_map_types.h"
#include "collision_queries.h"
#include "math/q_math.h"

#include <stdint.h>


extern collisionLeaf_t *cm_leafs;
extern int32_t *cm_leafbrushes;
extern collisionBrush_t *cm_brushes;

/*
 * Complete shared point-contents subsystem:
 *
 *   CoDUOMP.exe   CM_PointContents            0x00425970
 *                 CM_TransformedPointContents 0x00425a60
 *   coduo_lnxded  CM_PointContents            0x080575a9
 *                 CM_TransformedPointContents 0x0805772b
 *
 * Both targets select the same leaves, brushes, planes, and contents bits.
 * The Windows expressions retain the genuine x87 order difference:
 * Windows evaluates three-lane dot products as (Z+Y)+X, while Linux evaluates
 * them as (X+Y)+Z.  Every transformed coordinate is stored to binary32 before
 * CM_PointContents consumes it.
 */

int32_t CM_PointContents(const vec3_t point, int32_t modelHandle)
{
    const collisionLeaf_t *leaf;
    if (modelHandle != CM_WORLD_MODEL) {
        leaf = &CM_ClipHandleToModel(modelHandle)->leaf;
    } else {
        leaf = &cm_leafs[CM_PointLeafnum(point)];
    }

    int32_t contents = 0;
    for (int32_t leafBrushIndex = 0;
         leafBrushIndex < (int32_t)leaf->numLeafBrushes;
         ++leafBrushIndex) {
        const int32_t brushIndex =
            cm_leafbrushes[leaf->firstLeafBrush + leafBrushIndex];
        const collisionBrush_t *const brush = &cm_brushes[brushIndex];
        qboolean outside = qfalse;

        for (int32_t axis = 0; axis < 3; ++axis) {
            if (point[axis] < brush->mins[axis] ||
                brush->maxs[axis] < point[axis]) {
                outside = qtrue;
                break;
            }
        }
        if (outside != qfalse) {
            continue;
        }

        const collisionBrushSide_t *side = brush->nonAxialSides;
        for (int32_t sideCount = brush->nonAxialSideCount;
             sideCount != 0;
             --sideCount, ++side) {
            const cplane_t *const plane = side->plane;
            long double distance;
            distance =
                ((long double)plane->normal[2] * (long double)point[2] +
                 (long double)plane->normal[1] * (long double)point[1]) +
                (long double)plane->normal[0] * (long double)point[0];
            if (distance > (long double)plane->dist) {
                outside = qtrue;
                break;
            }
        }

        if (outside == qfalse) {
            contents |= brush->contents;
        }
    }

    return contents;
}

int32_t CM_TransformedPointContents(const vec3_t point, int32_t modelHandle,
                                    const vec3_t origin, const vec3_t angles)
{
    vec3_t transformed = {
        point[0] - origin[0],
        point[1] - origin[1],
        point[2] - origin[2]
    };

    if (modelHandle != CM_TEMP_BOX_MODEL_HANDLE &&
        (angles[0] != 0.0f || angles[1] != 0.0f || angles[2] != 0.0f)) {
        vec3_t forward;
        vec3_t right;
        vec3_t up;
        const vec3_t translated = {
            transformed[0], transformed[1], transformed[2]
        };

        AngleVectors(angles, forward, right, up);

        transformed[0] = (float)(
            ((long double)translated[2] * (long double)forward[2] +
             (long double)translated[1] * (long double)forward[1]) +
            (long double)translated[0] * (long double)forward[0]);
        transformed[1] = (float)-(
            ((long double)translated[2] * (long double)right[2] +
             (long double)translated[1] * (long double)right[1]) +
            (long double)translated[0] * (long double)right[0]);
        transformed[2] = (float)(
            ((long double)translated[2] * (long double)up[2] +
             (long double)translated[1] * (long double)up[1]) +
            (long double)translated[0] * (long double)up[0]);
    }

    return CM_PointContents(transformed, modelHandle);
}
