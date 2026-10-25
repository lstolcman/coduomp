#include "collision_terrain_trace.h"

#include <math.h>
#include <stdint.h>

extern int32_t cm_checkcount;

/*
 * Swept sphere/capsule trace against triangle soup:
 *
 *   CoDUOMP.exe  0x00424930..0x00425117
 *   coduo_lnxded 0x080558dd..0x08056772
 *
 * The complete authoritative bodies retain their original operation graphs.
 */
/* Source: CoDUOMP.exe 0x00424930..0x00425117.
 * Evidence: coduomp/mcode/CoDUOMP/FUN_00424930_00425117.mcode.
 * Name: exact same-module Mac symbol CM_TraceSphereThroughTerrainCollide.
 * Triangle-face crossings use the expanded radius; barycentric misses select
 * deduplicated finite edges or vertices for quadratic swept-sphere tests. */
void CM_TraceSphereThroughTerrainCollide(
    traceWork_t *traceWork,
    const collisionTriangleSoup_t *terrainCollide)
{
    const long double cylinderHalfHeightRaw =
        (long double)traceWork->sphere.halfheight -
        traceWork->sphere.radius;
    float verticalSpan;
    if (terrainCollide->negativeZOnly != 0) {
        verticalSpan = (float)(
            -2.0L * cylinderHalfHeightRaw);
        traceWork->start[2] = (float)(
            (long double)traceWork->start[2] +
            cylinderHalfHeightRaw);
        traceWork->end[2] = (float)(
            (long double)traceWork->end[2] +
            cylinderHalfHeightRaw);
    } else {
        verticalSpan = (float)(
            2.0L * cylinderHalfHeightRaw);
        traceWork->start[2] = (float)(
            (long double)traceWork->start[2] -
            cylinderHalfHeightRaw);
        traceWork->end[2] = (float)(
            (long double)traceWork->end[2] -
            cylinderHalfHeightRaw);
    }

    const float expandedRadius =
        traceWork->sphere.radius + 0.125f;
    const float negativeExpandedRadius =
        -expandedRadius;
    const float fractionEpsilon =
        0.000009999999747378752f;

    for (int32_t triangleIndex = 0;
         triangleIndex <
             (int32_t)terrainCollide->triangleCount;
         ++triangleIndex) {
        const collisionSoupTriangle_t *const
            triangle =
                &terrainCollide
                     ->triangles[triangleIndex];
        const long double endDistanceRaw =
            ((long double)triangle->plane.components.normal[2] *
                 traceWork->end[2] +
             (long double)triangle->plane.components.normal[1] *
                 traceWork->end[1]) +
            (long double)triangle->plane.components.normal[0] *
                traceWork->end[0] -
            triangle->plane.components.distance;
        if (!(endDistanceRaw < expandedRadius ||
              isunordered(endDistanceRaw,
                          expandedRadius)))
            continue;

        const long double startDistanceRaw =
            ((long double)triangle->plane.components.normal[1] *
                 traceWork->start[1] +
             (long double)triangle->plane.components.normal[0] *
                 traceWork->start[0]) +
            (long double)triangle->plane.components.normal[2] *
                traceWork->start[2] -
            triangle->plane.components.distance;
        const float startDistance =
            (float)startDistanceRaw;
        const float distanceDelta =
            (float)(startDistanceRaw -
                    endDistanceRaw);
        if (!(distanceDelta > 0.0f ||
              isunordered(distanceDelta, 0.0f)))
            continue;

        if (!(startDistance >
                  negativeExpandedRadius ||
              isunordered(startDistance,
                          negativeExpandedRadius))) {
            const long double oppositeCapDistance =
                (long double)verticalSpan *
                    triangle->plane.components.normal[2] +
                startDistance;
            if (!(oppositeCapDistance >
                      negativeExpandedRadius ||
                  isunordered(
                      oppositeCapDistance,
                      negativeExpandedRadius))) {
                continue;
            }

            const float inverseNormalZ =
                1.0f /
                triangle->plane.components.normal[2];
            const float sAtStart = (float)(
                ((long double)triangle->svec[1] *
                     traceWork->start[1] +
                 (long double)triangle->svec[0] *
                     traceWork->start[0]) +
                (long double)triangle->svec[2] *
                    traceWork->start[2] -
                triangle->svec[3]);
            const float tAtStart = (float)(
                ((long double)triangle->tvec[1] *
                     traceWork->start[1] +
                 (long double)triangle->tvec[0] *
                     traceWork->start[0]) +
                (long double)triangle->tvec[2] *
                    traceWork->start[2] -
                triangle->tvec[3]);

            const long double firstZOffset =
                ((long double)negativeExpandedRadius -
                 startDistance) *
                inverseNormalZ;
            const long double firstSRaw =
                firstZOffset *
                    triangle->svec[2] +
                sAtStart;
            const float firstS =
                (float)firstSRaw;
            const long double firstT =
                firstZOffset *
                    triangle->tvec[2] +
                tAtStart;
            if (firstSRaw >= 0.0f &&
                firstT >= 0.0f &&
                (long double)firstS + firstT <=
                    1.0f) {
                traceWork->trace.normal[0] =
                    triangle->plane.components.normal[0];
                traceWork->trace.normal[1] =
                    triangle->plane.components.normal[1];
                traceWork->trace.normal[2] =
                    triangle->plane.components.normal[2];
                traceWork->trace.fraction = 0.0f;
                traceWork->trace.startsolid =
                    qtrue;
                return;
            }

            const long double secondZOffset =
                !isunordered(oppositeCapDistance,
                             expandedRadius) &&
                        oppositeCapDistance <
                            expandedRadius
                    ? (long double)verticalSpan
                    : ((long double)expandedRadius -
                       startDistance) *
                          inverseNormalZ;
            const long double secondSRaw =
                secondZOffset *
                    triangle->svec[2] +
                sAtStart;
            const float secondS =
                (float)secondSRaw;
            const long double secondT =
                secondZOffset *
                    triangle->tvec[2] +
                tAtStart;
            /* 0x00424b45..0x00424b81 uses the asymmetric x87 status
             * tests from the original: the second-cap candidate is rejected
             * only by ordered s < 0, t < 0, or s+t > 1 comparisons. */
            if (!(secondSRaw < 0.0f) &&
                !(secondT < 0.0f) &&
                !((long double)secondS + secondT >
                  1.0f)) {
                traceWork->trace.normal[0] =
                    triangle->plane.components.normal[0];
                traceWork->trace.normal[1] =
                    triangle->plane.components.normal[1];
                traceWork->trace.normal[2] =
                    triangle->plane.components.normal[2];
                traceWork->trace.fraction = 0.0f;
                traceWork->trace.startsolid =
                    qtrue;
                return;
            }
            continue;
        }

        long double faceFraction;
        long double facePoint[3];
        if (!isunordered(startDistance,
                         expandedRadius) &&
            startDistance < expandedRadius) {
            faceFraction = 0.0L;
            facePoint[0] = traceWork->start[0];
            facePoint[1] = traceWork->start[1];
            facePoint[2] = traceWork->start[2];
        } else {
            faceFraction =
                ((long double)startDistance -
                 expandedRadius) /
                distanceDelta;
            if (!isunordered(
                    faceFraction,
                    traceWork->trace.fraction) &&
                faceFraction >
                    traceWork->trace.fraction) {
                continue;
            }
            for (int32_t axis = 0; axis < 3;
                 ++axis) {
                facePoint[axis] =
                    (long double)traceWork->start[axis] +
                    faceFraction *
                        traceWork->delta[axis];
            }
        }

        const float faceS = (float)(
            ((long double)facePoint[1] *
                 triangle->svec[1] +
             (long double)facePoint[0] *
                 triangle->svec[0]) +
            (long double)facePoint[2] *
                triangle->svec[2] -
            triangle->svec[3]);
        const long double faceT =
            ((long double)facePoint[2] *
                 triangle->tvec[2] +
             (long double)facePoint[1] *
                 triangle->tvec[1]) +
            (long double)facePoint[0] *
                triangle->tvec[0] -
            triangle->tvec[3];

        int32_t outsideMask = 0;
        if ((long double)faceS + faceT > 1.0f)
            outsideMask |= 1;
        if (faceS < 0.0f)
            outsideMask |= 2;
        if (faceT < 0.0f)
            outsideMask |= 4;

        if (outsideMask == 0) {
            traceWork->trace.normal[0] =
                triangle->plane.components.normal[0];
            traceWork->trace.normal[1] =
                triangle->plane.components.normal[1];
            traceWork->trace.normal[2] =
                triangle->plane.components.normal[2];
            if (faceFraction <=
                fractionEpsilon) {
                traceWork->trace.fraction = 0.0f;
                traceWork->trace.startsolid =
                    qtrue;
                return;
            }
            traceWork->trace.fraction =
                (float)(
                    faceFraction -
                    fractionEpsilon);
            continue;
        }

        const long double radiusSquared =
            (long double)expandedRadius *
            expandedRadius;
        for (int32_t corner = 0;
             corner < CM_TRIANGLE_VERTEX_COUNT;
             ++corner) {
            long double featureFraction;
            vec3_t featureNormal;

            if ((outsideMask &
                 (1 << corner)) != 0) {
                collisionSoupEdge_t *const edge =
                    triangle->oppositeEdges[corner];
                if (edge == NULL ||
                    edge->checkcount ==
                        cm_checkcount) {
                    continue;
                }
                edge->checkcount = cm_checkcount;

                /* Keep all three differences live through the edge projections;
                 * the first binary32 stores are the projected coordinates. */
                long double relativeStart[3];
                for (int32_t axis = 0;
                     axis < 3; ++axis) {
                    relativeStart[axis] =
                        (long double)traceWork->start[axis] -
                        edge->origin[axis];
                }
                const float radialStart0 =
                    (float)(
                        ((long double)relativeStart[0] *
                             edge->radialAxes[0][0] +
                         (long double)relativeStart[2] *
                             edge->radialAxes[0][2]) +
                        (long double)relativeStart[1] *
                            edge->radialAxes[0][1]);
                const float radialStart1 =
                    (float)(
                        ((long double)relativeStart[0] *
                             edge->radialAxes[1][0] +
                         (long double)relativeStart[2] *
                             edge->radialAxes[1][2]) +
                        (long double)relativeStart[1] *
                            edge->radialAxes[1][1]);
                const float edgeStart =
                    (float)(
                        ((long double)relativeStart[2] *
                             edge->unitDirection[2] +
                         (long double)relativeStart[1] *
                             edge->unitDirection[1]) +
                        (long double)relativeStart[0] *
                            edge->unitDirection[0]);
                const long double radialConstantRaw =
                        (long double)radialStart1 *
                            radialStart1 +
                        (long double)radialStart0 *
                            radialStart0 -
                        radiusSquared;
                const float radialConstant =
                    (float)radialConstantRaw;

                if (radialConstantRaw <= 0.0f) {
                    if (edgeStart < 0.0f ||
                        edgeStart > edge->length) {
                        continue;
                    }
                    traceWork->trace.normal[0] =
                        triangle->plane.components.normal[0];
                    traceWork->trace.normal[1] =
                        triangle->plane.components.normal[1];
                    traceWork->trace.normal[2] =
                        triangle->plane.components.normal[2];
                    traceWork->trace.fraction =
                        0.0f;
                    traceWork->trace.startsolid =
                        qtrue;
                    return;
                }

                const float radialDelta0 =
                    (float)(
                        ((long double)traceWork->delta[0] *
                             edge->radialAxes[0][0] +
                         (long double)traceWork->delta[1] *
                             edge->radialAxes[0][1]) +
                        (long double)traceWork->delta[2] *
                            edge->radialAxes[0][2]);
                const float radialDelta1 =
                    (float)(
                        ((long double)traceWork->delta[1] *
                             edge->radialAxes[1][1] +
                         (long double)traceWork->delta[0] *
                             edge->radialAxes[1][0]) +
                        (long double)traceWork->delta[2] *
                            edge->radialAxes[1][2]);
                const long double radialLinearRaw =
                        (long double)radialStart1 *
                            radialDelta1 +
                        (long double)radialStart0 *
                            radialDelta0;
                const float radialLinear =
                    (float)radialLinearRaw;
                if (!(radialLinearRaw < 0.0f ||
                      isunordered(radialLinearRaw,
                                  0.0f)))
                    continue;

                const long double radialQuadratic =
                    (long double)radialDelta1 *
                        radialDelta1 +
                    (long double)radialDelta0 *
                        radialDelta0;
                const long double discriminant =
                    (long double)radialLinear *
                        radialLinear -
                    radialQuadratic *
                        radialConstant;
                if (!(discriminant > 0.0L ||
                      isunordered(discriminant, 0.0L)))
                    continue;

                const long double
                    featureFractionRaw =
                    (-sqrtl(discriminant) -
                     radialLinear) /
                    radialQuadratic;
                featureFraction =
                    (float)featureFractionRaw;
                if (!(featureFractionRaw <
                      traceWork->trace.fraction)) {
                    continue;
                }

                const long double edgeAtHit =
                    (((long double)traceWork->delta[2] *
                          edge->unitDirection[2] +
                      (long double)traceWork->delta[0] *
                          edge->unitDirection[0]) +
                     (long double)traceWork->delta[1] *
                         edge->unitDirection[1]) *
                        featureFraction +
                    edgeStart;
                if (edgeAtHit < 0.0L ||
                    edgeAtHit > edge->length) {
                    continue;
                }

                const long double inverseRadius =
                    1.0L / expandedRadius;
                const long double normalAxis0 =
                    ((long double)radialDelta0 *
                         featureFraction +
                     radialStart0) *
                    inverseRadius;
                const float normalAxis1 = (float)(
                    ((long double)radialDelta1 *
                         featureFraction +
                     radialStart1) *
                    inverseRadius);
                for (int32_t axis = 0;
                     axis < 3; ++axis) {
                    const float axis0Term = (float)(
                        normalAxis0 *
                        edge->radialAxes[0][axis]);
                    featureNormal[axis] = (float)(
                        (long double)axis0Term +
                        (long double)normalAxis1 *
                            edge->radialAxes[1][axis]);
                }
            } else {
                collisionSoupVertex_t *const vertex =
                    triangle->vertices[corner];
                if (vertex == NULL ||
                    vertex->checkcount ==
                        cm_checkcount) {
                    continue;
                }
                vertex->checkcount =
                    cm_checkcount;

                vec3_t relativeStart;
                for (int32_t axis = 0;
                     axis < 2; ++axis) {
                    relativeStart[axis] =
                        traceWork->start[axis] -
                        vertex->position[axis];
                }
                /* FST retains the unrounded Z difference for the first product;
                 * the other factor and subsequent uses reload its float slot. */
                const long double relativeStartZRaw =
                    (long double)traceWork->start[2] - vertex->position[2];
                relativeStart[2] = (float)relativeStartZRaw;
                const long double vertexConstantRaw =
                        (relativeStartZRaw *
                             relativeStart[2] +
                         (long double)relativeStart[1] *
                             relativeStart[1]) +
                        (long double)relativeStart[0] *
                            relativeStart[0] -
                        radiusSquared;
                const float vertexConstant =
                    (float)vertexConstantRaw;
                if (vertexConstantRaw <= 0.0f) {
                    traceWork->trace.normal[0] =
                        triangle->plane.components.normal[0];
                    traceWork->trace.normal[1] =
                        triangle->plane.components.normal[1];
                    traceWork->trace.normal[2] =
                        triangle->plane.components.normal[2];
                    traceWork->trace.fraction =
                        0.0f;
                    traceWork->trace.startsolid =
                        qtrue;
                    return;
                }

                const long double vertexLinearRaw =
                        ((long double)relativeStart[1] *
                             traceWork->delta[1] +
                         (long double)relativeStart[0] *
                             traceWork->delta[0]) +
                        (long double)relativeStart[2] *
                            traceWork->delta[2];
                const float vertexLinear =
                    (float)vertexLinearRaw;
                if (!(vertexLinearRaw < 0.0f ||
                      isunordered(vertexLinearRaw,
                                  0.0f)))
                    continue;

                const long double discriminant =
                    (long double)vertexLinear *
                        vertexLinear -
                    (long double)
                        traceWork
                            ->deltaLengthSquared *
                        vertexConstant;
                if (!(discriminant >= 0.0L ||
                      isunordered(discriminant, 0.0L)))
                    continue;

                featureFraction =
                    (-sqrtl(discriminant) -
                     vertexLinear) /
                    traceWork
                        ->deltaLengthSquared;
                if (!(featureFraction <
                      traceWork->trace.fraction)) {
                    continue;
                }

                const long double inverseRadius =
                    1.0L / expandedRadius;
                for (int32_t axis = 0;
                     axis < 3; ++axis) {
                    featureNormal[axis] = (float)(
                        ((long double)traceWork
                             ->delta[axis] *
                             featureFraction +
                         relativeStart[axis]) *
                        inverseRadius);
                }
            }

            traceWork->trace.normal[0] =
                featureNormal[0];
            traceWork->trace.normal[1] =
                featureNormal[1];
            traceWork->trace.normal[2] =
                featureNormal[2];
            /* Preserve this recovered boundary's validated input, state, and compatibility invariants. */
            if (traceWork->trace.fraction <=
                fractionEpsilon) {
                traceWork->trace.fraction = 0.0f;
                traceWork->trace.startsolid =
                    qtrue;
                return;
            }
            traceWork->trace.fraction =
                (float)(
                    featureFraction -
                    fractionEpsilon);
        }
    }
}
