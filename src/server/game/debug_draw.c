/*
 * Source reconstruction for debug drawing helper functions.
 *
 * Binary SHA-256:
 * 4d2391c10e234559ace294fb69d96741cad522653c361f13f626f4dc1891acb7
 */

#include <math.h>
#include <stdint.h>

#include "game_functions.h"
#include "game_globals.h"
#include "level_locals.h"
#include "recovered_game.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87_eval.h"

#define DEBUG_CIRCLE_SEGMENTS 16
#define DEBUG_CIRCLE_STEP_RADIANS 0.39269908169872414f /* original float 0x3ec90fdb */
#define DEBUG_PI 3.141592653589793f /* original float 0x40490fdb */
#define DEBUG_ARC_INTERVAL_SCALE 0.06666667f /* original float 0x3d888889 */
#define DEBUG_ARC_DEGREE_SCALE 0.0055555557f /* original float 0x3bb60b61 */
#define DEBUG_DEGREES_PER_CIRCLE 360.0f

void trap_AddDebugLine(const float *start, const float *end, const float *color,
                              int depthTest, int duration);

static const int debugBoxEdges[12][2] = {
    {0, 1},
    {0, 2},
    {0, 4},
    {1, 3},
    {1, 5},
    {2, 3},
    {2, 6},
    {3, 7},
    {4, 5},
    {4, 6},
    {5, 7},
    {6, 7},
};

/* NOT_FROM_ORIGINAL_SOURCE: local extraction of G_DebugCircle view-origin setup. */
static void game_compat_debug_view_origin(vec3_t origin)
{
    const gclient_t *client = level.clients;

    origin[0] = client->ps.psOrigin[0];
    origin[1] = client->ps.psOrigin[1];
    origin[2] = client->ps.psOrigin[2] + client->ps.viewHeightCurrent;
}

/* VERIFIED_DECOMPILER(0x51258, 61258_G_DebugLine.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - trap_AddDebugLine wrapper forwards all five arguments in order. */
/* 0x51258 G_DebugLine */
void G_DebugLine(const float *start, const float *end, const float *color, int depthTest,
                 int duration)
{
    trap_AddDebugLine(start, end, color, depthTest, duration);
}

/* VERIFIED_DECOMPILER(0x51297, 61297_G_DebugBox.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - eight corner stores, mins/maxs bit selection, 12 edge-table lookups, and G_DebugLine arguments checked. */
/* 0x51297 G_DebugBox */
void G_DebugBox(const float *mins, const float *maxs, const float *color, int depthTest,
                int duration)
{
    vec3_t corners[8];

    for (int corner = 0; corner < 8; corner++) {
        for (int axis = 0; axis < 3; axis++) {
            corners[corner][axis] = ((corner >> axis) & 1) ? maxs[axis] : mins[axis];
        }
    }

    for (int edge = 0; edge < 12; edge++) {
        G_DebugLine(corners[debugBoxEdges[edge][0]], corners[debugBoxEdges[edge][1]],
                    color, depthTest, duration);
    }
}

/* VERIFIED_DECOMPILER(0x518ec, 618ec_FUN_000618ec.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - x87 cos/sin sequence and sine/cosine output pointer order checked. */
/* 0x518ec FUN_000618ec */
/* Stock 0x518f9 is `fld DWORD; fsincos; fstp(cos); fstp(sin)`.  Native x87
 * targets use that exact sequence; only non-x87 targets use the permitted libm
 * fallback. */
static void DebugSinCos(float radians, float *sineOut, float *cosineOut)
{
#if defined(__i386__) || defined(__x86_64__)
    coduo_x87_sincosf(radians, sineOut, cosineOut);
#else
    long double angle = (long double)radians;

    *cosineOut = (float)cosl(angle);
    *sineOut = (float)sinl(angle);
#endif
}

/* VERIFIED_DECOMPILER(0x51465, 61465_G_DebugCircleEx.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - normal basis setup, 16-point sin/cos loop, radius scaling, point stores, and closed line loop checked. */
/* 0x51465 G_DebugCircleEx */
void G_DebugCircleEx(const float *center, float radius, const float *normal,
                     const float *color, int depthTest, int duration)
{
    vec3_t normalizedNormal;
    vec3_t tangent;
    vec3_t bitangent;
    vec3_t points[DEBUG_CIRCLE_SEGMENTS];

    VectorNormalize2(normal, normalizedNormal);
    PerpendicularVector(tangent, normalizedNormal);
    CrossProduct(normalizedNormal, tangent, bitangent);

    for (int index = 0; index < DEBUG_CIRCLE_SEGMENTS; index++) {
        float sine;
        float cosine;
        /* The angle rounds to float before sin/cos. Windows uses a float
         * step; Linux (0x514e3) uses the double step. */
        float radians = (float)((WIDE_FLOAT)index * DEBUG_CIRCLE_STEP_RADIANS);

        DebugSinCos(radians, &sine, &cosine);
        sine *= radius;
        cosine *= radius;

        /* Linux (0x51554..0x51697) stores center+bitangent*sine before
         * adding tangent*cosine. Windows combines tangent*cosine and
         * bitangent*sine, adds center, then performs the single float store. */
        for (int c = 0; c < 3; c++) {
            WIDE_FLOAT tangentPart = (WIDE_FLOAT)tangent[c] * cosine;
            WIDE_FLOAT bitangentPart = (WIDE_FLOAT)bitangent[c] * sine;
            points[index][c] = (tangentPart + bitangentPart) + center[c];
        }
    }

    for (int index = 0; index < DEBUG_CIRCLE_SEGMENTS; index++) {
        G_DebugLine(points[index], points[(index + 1) & 0xf], color, depthTest, duration);
    }
}

/* VERIFIED_DECOMPILER(0x5139d, 6139d_G_DebugCircle.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - view-origin normal path, up-normal branch, level/client offsets, and G_DebugCircleEx argument order checked. */
/* 0x5139d G_DebugCircle */
void G_DebugCircle(const float *center, float radius, const float *color, int depthTest,
                   qboolean useUpNormal, int duration)
{
    vec3_t normal;

    if (useUpNormal == 0) {
        vec3_t viewOrigin;

        game_compat_debug_view_origin(viewOrigin);
        normal[0] = center[0] - viewOrigin[0];
        normal[1] = center[1] - viewOrigin[1];
        normal[2] = center[2] - viewOrigin[2];
    } else {
        normal[0] = 0.0f;
        normal[1] = 0.0f;
        normal[2] = 1.0f;
    }

    G_DebugCircleEx(center, radius, normal, color, depthTest, duration);
}

/* VERIFIED_DECOMPILER(0x51725, 61725_G_DebugArc.c, VERIFY-DEBUG-DRAW-PACKET-2026-06-17): DATAFLOW_VERIFIED - step calculation, negative-range adjustment, radian conversion constants, 16 point stores, and 15 segment draw loop checked. */
/* 0x51725 G_DebugArc */
void G_DebugArc(const float *center, float radius, float startAngle, float endAngle,
                const float *color, int depthTest, int duration)
{
    vec3_t points[DEBUG_CIRCLE_SEGMENTS];
    /* Linux divides the live angle difference by 15 and stores the step.
     * Windows multiplies by the float reciprocal and compares the live
     * result before reloading the float step for point generation. */
    WIDE_FLOAT stepForSign = ((WIDE_FLOAT)endAngle - startAngle) * DEBUG_ARC_INTERVAL_SCALE;
    float step = (float)stepForSign;

    if (stepForSign < 0.0f) {
        startAngle -= DEBUG_DEGREES_PER_CIRCLE;
        step = (float)(((WIDE_FLOAT)endAngle - startAngle) * DEBUG_ARC_INTERVAL_SCALE);
    }

    for (int index = 0; index < DEBUG_CIRCLE_SEGMENTS; index++) {
        float sine;
        float cosine;
        /* Linux (0x5179d..0x517bc) uses double pi and division by 180.
         * Windows uses float pi followed by the float reciprocal of 180.
         * Both retain intermediates through the final float angle store. */
        float radians = (float)((((WIDE_FLOAT)index * step + startAngle) * DEBUG_PI) * DEBUG_ARC_DEGREE_SCALE);

        DebugSinCos(radians, &sine, &cosine);
        /* Both platforms retain trig*radius through the center addition,
         * then store the completed coordinate as float. */
        points[index][0] = (float)((WIDE_FLOAT)center[0] + (WIDE_FLOAT)cosine * radius);
        points[index][1] = (float)((WIDE_FLOAT)center[1] + (WIDE_FLOAT)sine * radius);
        points[index][2] = center[2];
    }

    for (int index = 0; index < DEBUG_CIRCLE_SEGMENTS - 1; index++) {
        G_DebugLine(points[index], points[index + 1], color, depthTest, duration);
    }
}
