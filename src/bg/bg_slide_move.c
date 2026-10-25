#include "bg_pmove.h"

#include "bg_player_state.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_native_x87.h"
#include "compat/coduo_x87_eval.h"
#include "math/q_math.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>


void Com_Printf(const char *format, ...);

/*
 * The Windows cgame/game helpers are instruction-identical:
 *
 *   uo_cgame_mp_x86.dll  0x3000e810..0x3000e831
 *   uo_game_mp_x86.dll   0x2000e5c0..0x2000e5e1
 *
 * They add the binary64 2^-30 tie-breaking bias to a binary32 input and use a
 * nearest-mode FISTP m32. The cgame also calls this original helper from its
 * shell-shock parameter loader, so it remains externally visible rather than
 * being reduced to a PM_StepSlideMove-local extraction.
 */
int32_t Script_RoundToNearestInt(float value)
{
    const double bias = 9.313225746154785e-10;
    const long double biased = (long double)value + (long double)bias;

    return coduo_x87_fistp_i32(biased);
}

/*
 * The authoritative Windows cgame/game bodies are instruction-identical after
 * relocation normalization:
 *
 *   uo_cgame_mp_x86.dll  PM_SlideMove     0x3000e930..0x3000f213
 *   uo_game_mp_x86.dll   PM_SlideMove     0x2000e6e0..0x2000efc2
 *   uo_cgame_mp_x86.dll  PM_StepSlideMove 0x3000f220..0x3000fa05
 *   uo_game_mp_x86.dll   PM_StepSlideMove 0x2000efd0..0x2000f7b4
 *
 * Linux game keeps the same state machines, but its unoptimized source stores
 * additional binary32 intermediates and uses a distinct step-delta rounding
 * helper. The maintained bodies retain the Windows arithmetic graph
 * throughout this coupled movement subsystem.
 */
// Source: uo_cgame_mp_x86.dll 0x3000e930..0x3000f213
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000e930_3000f213.mcode
//
// int PM_SlideMove(qboolean gravity)
//
// The Quake3/CoD pmove collision "slide" core. Given the current player-state
// origin and velocity (in the global pmove context pm, whose ->anim is
// the playerState the movers read/write), it advances the player through the world
// in up to MAX_BUMPS (4) collision "bumps": each bump traces from the current
// origin along the remaining velocity; if it hits a surface it clips the velocity
// into that surface's plane (accumulating a plane list) and keeps sliding with the
// leftover time. On a full clear (trace.fraction == 1) it stops early. It returns
// qtrue when the move was blocked by a plane this substep (so PM_StepSlideMove will
// try stepping), qfalse when it slid freely.
//
// NAME ADJUDICATION: the .mcode header assigns "PM_StepSlideMove" purely by size
// match (win 0x8e3 vs matched 0x8b0) — REJECTED per the no-size-naming rule. That
// is the OUTER step wrapper; the real PM_StepSlideMove is 0x3000f220 and its
// already-reconstructed .c file (FUN_3000f220_3000fa05.c) calls THIS function as
// "PM_SlideMove(gravity)". PM_SlideMove is proven by:
//   * the debug .rdata strings this function pushes to Com_Printf (0x3002b420):
//       "%i:MAX_CLIP_PLANES\n"                               (0x30074bd8)
//       "%i:recollided with plane normal (%.2f, %.2f, %.2f)\n" (0x30074ba4)
//     ("%i" = c_pmove, 0x30134cd0), gated on pm->debugMove >= 2;
//   * the OVERCLIP 1.001f (0x3007bf74) reflect scale and the MAX_CLIP_PLANES loop;
//   * the callee graph: PM_trace (0x30008280), PM_ClipVelocity (0x30008390),
//     VectorNormalize2 (0x30049920) and VectorNormalize (0x30049700).
// This is the canonical Quake3 PM_SlideMove.
//
// ABI: single cdecl arg `gravity` at [entry_esp+4] (read at [ESP+0x10c] before the
// ESI/EDI pushes and at [ESP+0x114] after them — the same slot at two frame
// depths); result in AL via SETNZ. Ends in a plain RET (caller cleans the one
// dword). EBP starts as pm, but the machine reloads that global after
// every PM_trace and after either debug Com_Printf call.  Reads of ->ps are also
// repeated at the individual access sites instead of being retained across calls.
// Register args to the register-ABI callees are re-derived from this function's own
// bytes at each call site, not from the callees' provisional decls.

/* MAX_BUMPS: the pmove slide/bump iteration cap (0x3000f112 CMP EAX,4). */
enum { PM_SLIDEMOVE_MAX_BUMPS = 4 };

/* MAX_CLIP_PLANES: the clip-plane list cap. When numplanes reaches this the debug
 * "%i:MAX_CLIP_PLANES\n" is emitted and the move gives up (0x3000eb79 CMP ESI,8;
 * JGE fail path). */
enum { PM_SLIDEMOVE_MAX_CLIP_PLANES = 8 };

/* OVERCLIP (1.001f, 0x3007bf74 == 0x3f8020c5): the reflect scale PM_SlideMove and
 * PM_ClipVelocity push velocity out along the plane normal by, so the player
 * doesn't creep back into the surface. */
#define PM_OVERCLIP 1.001f

/* Plane-parallelism reject: two accumulated clip-plane normals whose dot exceeds
 * 0.999f (0x3007bf78) are treated as the same plane and skipped when re-clipping
 * (0x3000ebc1). */
#define PM_SLIDEMOVE_SAME_PLANE_DOT 0.999f

/* Into-plane speed epsilon (0.1, double @0x3007bd18): a clipped velocity whose
 * component into a plane exceeds this small negative bound counts as "still moving
 * into the plane", triggering the two-/three-plane recombination (0x3000ecb4,
 * 0x3000ee62, 0x3000f0a4). */
#define PM_SLIDEMOVE_INTO_PLANE_EPS 0.1

/* VectorNormalize2 (0x30049920): unit-normalizes `in` into a SEPARATE output vector
 * and returns the pre-normalization length in ST0 (discarded here via FSTP ST0).
 * At the call site in= EDI = &ps->velocity, out= ESI = &planes[numplanes][0]. The
 * shared decl already exists in client_recovered.h. */

/* PM_SlideMove uses pm->ps as the playerState. The shared header types
 * pmove_t.anim as playerState_t* (its animation-facing role), but the
 * pmove core reads/writes it as playerState_t (velocity +0x20, psOrigin +0x14,
 * gravity +0x40, pmTime +0x10, psClientNum +0xd4) — see the globals.h DIVERGENCE
 * note that `anim` is really pmove->ps. Access it through this typed view. */

int32_t PM_SlideMove(int32_t gravity)
{
    pmove_t *move = pm;                    /* EBP = [0x30539850] */
    playerState_t *ps = (playerState_t *)move->ps;    /* MOV EAX,[EBP] */

    /* MAX_CLIP_PLANES clip planes; each stored as a bare vec3 normal.
     * planes[] lives at [ESP+0xb0]; the code addresses element k as a mid-vector
     * pointer &planes[k][1] and reads [-4]/[0]/[+4] for [0]/[1]/[2]. */
    vec3_t planes[PM_SLIDEMOVE_MAX_CLIP_PLANES];

    vec3_t primal_velocity;   /* [ESP+0x60/0x64/0x68] — restored to ps->velocity if pmTime */
    vec3_t endVelocity;       /* [ESP+0x2c/0x30/0x34] — gravity end-of-frame velocity */
    vec3_t end;               /* [ESP+0x6c/0x70/0x74] — trace target this bump */
    vec3_t clipVelocity;      /* [ESP+0x20/0x24/0x28] */
    vec3_t endClipVelocity;   /* [ESP+0x48/0x4c/0x50] */
    vec3_t dir;               /* [ESP+0x3c/0x40/0x44] — crossproduct slide dir */
    trace_t trace;            /* [ESP+0x80..] */
    float   time_left;        /* [ESP+0x38] */
    int32_t numplanes;        /* [ESP+0x1c] */
    int32_t bumpcount;        /* [ESP+0x5c] */
    int32_t i, j, k;

    /* The three zero stores at 0x3000e943..0x3000e94b address end[] after the
     * later ESI/EDI pushes.  Each lane is overwritten before its first read, but
     * retain the original initialization operations. */
    end[2] = 0.0f;
    end[1] = 0.0f;
    end[0] = 0.0f;

    /* primal_velocity = ps->velocity. The velocity[0]/[2] stores at 0x3000e955
     * ([ESP+0x58]) and 0x3000e95c ([ESP+0x60]) execute BEFORE the ESI/EDI pushes
     * at 0x3000e967/e968, so in the final frame they are [ESP+0x60] and
     * [ESP+0x68]; velocity[1] is stored after the pushes (0x3000e981,
     * [ESP+0x64]). The tail pmTime restore reads exactly those final slots
     * (0x3000f152/f156/f166) — a plain VectorCopy. */
    memcpy(&primal_velocity[0], &ps->velocity[0], sizeof(float));
    memcpy(&primal_velocity[2], &ps->velocity[2], sizeof(float));

    /* endVelocity is cleared up front (0x3000e971..e97d clear [ESP+0x2c..0x44]). */
    endVelocity[0] = 0.0f;
    endVelocity[1] = 0.0f;
    endVelocity[2] = 0.0f;
    dir[0] = 0.0f;
    dir[1] = 0.0f;
    dir[2] = 0.0f;
    memcpy(&primal_velocity[1], &ps->velocity[1], sizeof(float));

    if (gravity) {                                       /* 0x3000e960 CMP [gravity]; JZ e9ef */
        float gravityFloat;

        /* FILD ps->gravity followed by FSTP m32 before it participates in the
         * multiplication. */
        gravityFloat = (float)ps->gravity;
        /* endVelocity = ps->velocity, then apply half of this frame's gravity. */
        memcpy(&endVelocity[0], &ps->velocity[0], sizeof(float));
        memcpy(&endVelocity[2], &ps->velocity[2], sizeof(float));
        memcpy(&endVelocity[1], &ps->velocity[1], sizeof(float));
        /* endVelocity[2] = ps->velocity[2] - ps->gravity * pml.frametime */
        endVelocity[2] = (float)((long double)ps->velocity[2]
            - (long double)gravityFloat * (long double)pml.frametime);
        /* ps->velocity[2] = (endVelocity[2] + ps->velocity[2]) * 0.5f  (0x3007bce8) */
        ps->velocity[2] = (float)(((long double)endVelocity[2]
            + (long double)ps->velocity[2]) * (long double)0.5f);
        memcpy(&primal_velocity[2], &endVelocity[2], sizeof(float));

        if (pml.groundPlane) {                            /* 0x3000e9cd CMP [0x305395b0]; JZ */
            /* Clip velocity into the ground plane so gravity doesn't push us in. */
            ps = (playerState_t *)move->ps;
            PM_ClipVelocity(ps->velocity, pml.groundTrace.normal, ps->velocity, PM_OVERCLIP);
        }
    }

    time_left = pml.frametime;                            /* 0x3000e9f5: [ESP+0x38] = pml.frametime */

    /* Seed the clip-plane list. When on a ground plane, plane[0] is the ground
     * normal and plane[1] is the (normalized) current velocity; else the list
     * starts empty and plane[0] is the normalized velocity. */
    if (pml.groundPlane) {                                /* 0x3000e9ef CMP [0x305395b0]; JZ ea32 */
        planes[0][0] = pml.groundTrace.normal[0];              /* 0x3000ea01..ea25: [ESP+0xb0..b8] */
        planes[0][1] = pml.groundTrace.normal[1];
        planes[0][2] = pml.groundTrace.normal[2];
        numplanes = 1;                                   /* 0x3000ea2c: [ESP+0x1c] = 1 */
    } else {
        numplanes = 0;                                   /* 0x3000ea32: [ESP+0x1c] = 0 */
    }

    /* plane[numplanes] = normalize(ps->velocity); numplanes++. VectorNormalize2 with
     * in = &ps->velocity (EDI), out = &planes[numplanes][0] (ESI); length discarded. */
    ps = (playerState_t *)move->ps;
    VectorNormalize2(ps->velocity, planes[numplanes]);   /* 0x3000ea38..ea48 */
    ++numplanes;                                         /* bounded to 1 or 2 here */

    for (bumpcount = 0; bumpcount < PM_SLIDEMOVE_MAX_BUMPS; ++bumpcount) {
        /* end = ps->psOrigin + time_left * ps->velocity */
        ps = (playerState_t *)move->ps;
        end[0] = (float)((long double)time_left * (long double)ps->velocity[0]
            + (long double)ps->psOrigin[0]);
        ps = (playerState_t *)move->ps;
        end[1] = (float)((long double)time_left * (long double)ps->velocity[1]
            + (long double)ps->psOrigin[1]);
        ps = (playerState_t *)move->ps;
        end[2] = (float)((long double)time_left * (long double)ps->velocity[2]
            + (long double)ps->psOrigin[2]);

        /* PM_trace(&trace, start = ps->psOrigin, move->mins, move->maxs, end,
         *          passEntityNum = ps->psClientNum, traceType = move->traceMask).
         * ABI at this site: EAX = move->traceMask (traceType), EBX = &trace, five
         * cdecl pushes (start, mins, maxs, end, passEntityNum). (0x3000eaae..eace) */
        ps = (playerState_t *)move->ps;
        {
            int32_t passEntityNum = (int32_t)ps->psClientNum;
            PM_trace(&trace, ps->psOrigin, move->mins, move->maxs, end,
                     passEntityNum, move->traceMask);
        }

        if (trace.allsolid) {                            /* 0x3000ead3 MOV AL,[ESP+0xc2]; TEST; JNZ f183 */
            /* Entirely stuck in a solid: kill vertical velocity and report blocked. */
            move = pm;
            ps = (playerState_t *)move->ps;
            ps->velocity[2] = 0.0f;                      /* 0x3000f18e: MOV [ps+0x28],0 */
            return 1;                                    /* 0x3000f195: EAX = 1 */
        }

        {
            long double traceFraction = (long double)trace.fraction;
            move = pm;                          /* 0x3000eaec */
            if (traceFraction > (long double)0.0f) {
            /* Moved some distance: commit the traced endpoint as the new origin. */
                ps = (playerState_t *)move->ps;
                memcpy(&ps->psOrigin[0], &trace.endpos[0], sizeof(float));
                ps = (playerState_t *)move->ps;
                memcpy(&ps->psOrigin[1], &trace.endpos[1], sizeof(float));
                ps = (playerState_t *)move->ps;
                memcpy(&ps->psOrigin[2], &trace.endpos[2], sizeof(float));
            }
        }

        {
            uint32_t fractionBits;

            memcpy(&fractionBits, &trace.fraction, sizeof(fractionBits));
            if (fractionBits == UINT32_C(0x3f800000)) {
                break;                                   /* moved the entire remaining distance */
            }
        }

        /* Record the entity we bumped in the pmove touch list (PM_AddTouchEnt),
         * unless it is ENTITYNUM_WORLD, the list is full (32), or already present. */
        if (trace.entityNum != ENTITYNUM_WORLD) { /* 0x3000eb37/eb3f: CMP 0x3fe */
            if (move->numtouch != PM_MAX_TOUCH_ENTS) {     /* 0x3000eb47 CMP [EBP+0x54],0x20 */
                int32_t already = 0;
                for (i = 0; i < move->numtouch; i++) {     /* 0x3000eb51..eb65 */
                    if (move->impactEntityNums[i] == (int32_t)trace.entityNum) { /* 0x3000eb58 */
                        already = 1;
                        break;
                    }
                }
                if (!already) {
                    move->impactEntityNums[move->numtouch] = (int32_t)trace.entityNum; /* 0x3000eb67 */
                    move->numtouch = coduo_int32_from_bits((uint32_t)move->numtouch + 1u);
                }
            }
        }

        /* time_left -= time_left * trace.fraction */
        time_left = (float)((long double)time_left
            - (long double)trace.fraction * (long double)time_left);

        if (numplanes >= PM_SLIDEMOVE_MAX_CLIP_PLANES) { /* 0x3000eb79 CMP ESI,8; JGE f1a2 */
            /* Too many planes: bail. With debug >= 2 print MAX_CLIP_PLANES, then
             * zero all velocity and report blocked. */
            if (move->debugMove >= 2) {                    /* 0x3000f1a2 CMP [EBP+0x38],2; JL f1c1 */
                Com_Printf("%i:MAX_CLIP_PLANES\n", c_pmove); /* 0x30074bd8 */
                move = pm;                      /* 0x3000f1b8 */
            }
            ps = (playerState_t *)move->ps;
            ps->velocity[2] = 0.0f;
            ps = (playerState_t *)move->ps;
            ps->velocity[1] = 0.0f;
            ps = (playerState_t *)move->ps;
            ps->velocity[0] = 0.0f;
            return 1;                                    /* 0x3000f1d8: EAX = 1 */
        }

        /* If the new plane is nearly parallel to a plane already in the list, merge
         * (nudge velocity out) instead of adding it. The first matching plane found
         * (dot > 0.999f) is the one used for the nudge. */
        {
            int32_t matched = -1;
            for (i = 0; i < numplanes; i++) {            /* 0x3000eba0..ebd4 */
                /* Dot summed x,z,y (FADDP order 0x3000eba0..ebbf) and compared
                 * UNROUNDED on the x87 stack (no float store before the FCOMP),
                 * so it must stay inline in the compare. */
                long double samePlaneDot =
                    ((long double)trace.normal[2] * (long double)planes[i][2]
                     + (long double)trace.normal[0] * (long double)planes[i][0])
                    + (long double)trace.normal[1] * (long double)planes[i][1];
                if (samePlaneDot > (long double)PM_SLIDEMOVE_SAME_PLANE_DOT) {
                    matched = i;
                    break;
                }
            }
            if (matched >= 0) {
                /* Merge: velocity += trace.normal; endVelocity += trace.normal.
                 * With debug >= 2 print the recollided normal. */
                if (move->debugMove >= 2) {                /* 0x3000ebd8 CMP [EBP+0x38],2; JL ec1a */
                    Com_Printf("%i:recollided with plane normal (%.2f, %.2f, %.2f)\n", /* 0x30074ba4 */
                               c_pmove,
                               (double)trace.normal[0],   /* 0x3000ebfc: [ESP+0xa8] = trace.normal[0] */
                               (double)trace.normal[1],   /* 0x3000ebf1: [ESP+0xac] = trace.normal[1] */
                               (double)trace.normal[2]);  /* 0x3000ebde: [ESP+0x98] = trace.normal[2] */
                    move = pm;                  /* 0x3000ec11 */
                }
                ps = (playerState_t *)move->ps;
                ps->velocity[0] = (float)((long double)trace.normal[0]
                    + (long double)ps->velocity[0]);
                ps = (playerState_t *)move->ps;
                ps->velocity[1] = (float)((long double)trace.normal[1]
                    + (long double)ps->velocity[1]);
                ps = (playerState_t *)move->ps;
                ps->velocity[2] = (float)((long double)trace.normal[2]
                    + (long double)ps->velocity[2]);
                continue;                                 /* 0x3000ec4c JL f10d: next bump */
            }
        }

        /* Add the new plane's normal to the list. */
        memcpy(&planes[numplanes][0], &trace.normal[0], sizeof(float));
        memcpy(&planes[numplanes][1], &trace.normal[1], sizeof(float));
        memcpy(&planes[numplanes][2], &trace.normal[2], sizeof(float));
        ++numplanes;                                     /* prior cap check proves <= 8 */

        /* Modify original velocity so it parallels the clip planes. Find the FIRST
         * plane the velocity is moving into (dot < 0.1); if none, this bump is done
         * (0x3000ec97..eccd: the i-loop breaks to the clip at ecd2 on the first hit,
         * else falls through to the next bump at f10d). */
        {
            int32_t offending = -1;
            float   offendingInto = 0.0f;                 /* the offending plane's into-dot ([ESP+0x18]) */
            ps = (playerState_t *)move->ps;
            for (i = 0; i < numplanes; i++) {            /* 0x3000ec97..eccd; i = EDX */
                /* Summed x,z,y (FADDP order 0x3000ec97..ecaa), one rounding at
                 * the [ESP+0x18] store. */
                float into = (float)(
                    ((long double)planes[i][0] * (long double)ps->velocity[0]
                     + (long double)planes[i][2] * (long double)ps->velocity[2])
                    + (long double)ps->velocity[1] * (long double)planes[i][1]);
                /* TEST AH,1 accepts both below and unordered x87 outcomes. */
                if (!(into >= PM_SLIDEMOVE_INTO_PLANE_EPS)) {
                    offending = i;                        /* 0x3000ecd6 stores i (EDX) to [ESP+0x58] */
                    offendingInto = into;                 /* value in [ESP+0x18] reused at ecd2 */
                    break;
                }
            }
            if (offending < 0) {
                continue;                                 /* no offending plane: next bump (0x3000eccd JMP f10d) */
            }
            i = offending;

            /* Track the worst (largest) negated into-plane speed as pml.maxClipImpact.
             * Reuses the offending plane's into-dot ([ESP+0x18]) negated (0x3000ecda FCHS). */
            {
                float negInto = -offendingInto;           /* 0x3000ecda FCHS */
                if (negInto > pml.maxClipImpact) {        /* 0x3000ece4 FCOMP [0x305395e8] */
                    pml.maxClipImpact = negInto;          /* 0x3000ecf5 MOV [0x305395e8] */
                }
            }

            /* Slide the current velocity along this plane (OVERCLIP reflect). */
            {
                /* backoff = (ps->velocity . planes[i]) * overbounce (FMUL or FDIV by
                 * OVERCLIP depending on the sign of the dot). Dot summed x,z,y
                 * (FADDP order 0x3000ed00..ed21). */
                ps = (playerState_t *)move->ps;             /* 0x3000ecfd */
                float dot = (float)(
                    ((long double)ps->velocity[2] * (long double)planes[i][2]
                     + (long double)ps->velocity[0] * (long double)planes[i][0])
                    + (long double)ps->velocity[1] * (long double)planes[i][1]);
                /* 0x3000ed2b..ed4a: JP (dot >= 0) -> FDIV 1.001; fall-through
                 * (dot < 0) -> FMUL 1.001 — canonical Q3 backoff scaling. */
                float backoff = (dot < 0.0f)
                    ? (float)((long double)dot * (long double)PM_OVERCLIP)
                    : (float)((long double)dot / (long double)PM_OVERCLIP);

                /* Each backoff*plane product is spilled to a float slot
                 * ([ESP+0x14]) before the subtract — two roundings per lane. */
                float scaled;
                scaled = (float)((long double)backoff * (long double)planes[i][0]);
                clipVelocity[0] = (float)((long double)ps->velocity[0] - (long double)scaled);
                scaled = (float)((long double)backoff * (long double)planes[i][1]);
                clipVelocity[1] = (float)((long double)ps->velocity[1] - (long double)scaled);
                scaled = (float)((long double)backoff * (long double)planes[i][2]);
                clipVelocity[2] = (float)((long double)ps->velocity[2] - (long double)scaled);

                /* endClipVelocity = endVelocity clipped by the same plane. */
                {
                    float dotE = (float)(
                        ((long double)endVelocity[0] * (long double)planes[i][0]
                         + (long double)endVelocity[1] * (long double)planes[i][1])
                        + (long double)endVelocity[2] * (long double)planes[i][2]);
                    /* 0x3000edaf..edce: same FMUL(neg)/FDIV(non-neg) select. */
                    float backoffE = (dotE < 0.0f)
                        ? (float)((long double)dotE * (long double)PM_OVERCLIP)
                        : (float)((long double)dotE / (long double)PM_OVERCLIP);
                    scaled = (float)((long double)backoffE * (long double)planes[i][0]);
                    endClipVelocity[0] = (float)((long double)endVelocity[0] - (long double)scaled);
                    scaled = (float)((long double)backoffE * (long double)planes[i][1]);
                    endClipVelocity[1] = (float)((long double)endVelocity[1] - (long double)scaled);
                    scaled = (float)((long double)backoffE * (long double)planes[i][2]);
                    endClipVelocity[2] = (float)((long double)endVelocity[2] - (long double)scaled);
                }
            }

            /* Recheck against every OTHER plane; if the clipped velocity still moves
             * into another plane, we need a two-plane (crossproduct) solution. */
            for (j = 0; j < numplanes; j++) {            /* 0x3000ee40..f0d1; j = EDX */
                if (j == i) {                             /* 0x3000ee40 CMP EDX,[ESP+0x58] (=i) */
                    continue;
                }
                /* This dot is compared UNROUNDED on the x87 stack (no float
                 * store before the FCOMP 0.1 double); retain the wide intermediate. */
                WIDE_FLOAT secondPlaneDot =
                    ((WIDE_FLOAT)clipVelocity[0] * (WIDE_FLOAT)planes[j][0]
                     + (WIDE_FLOAT)clipVelocity[1] * (WIDE_FLOAT)planes[j][1])
                    + (WIDE_FLOAT)clipVelocity[2] * (WIDE_FLOAT)planes[j][2];
                if (secondPlaneDot >= PM_SLIDEMOVE_INTO_PLANE_EPS) {
                    continue;                             /* still ok against plane j */
                }

                /* Slide the clipped velocity along the second plane too (OVERCLIP). */
                {
                    float dot2 = (float)(
                        ((long double)clipVelocity[0] * (long double)planes[j][0]
                         + (long double)clipVelocity[1] * (long double)planes[j][1])
                        + (long double)clipVelocity[2] * (long double)planes[j][2]);
                    /* 0x3000ee93..eeb2: FMUL(neg)/FDIV(non-neg) select. */
                    float backoff2 = (dot2 < 0.0f)
                        ? (float)((long double)dot2 * (long double)PM_OVERCLIP)
                        : (float)((long double)dot2 / (long double)PM_OVERCLIP);
                    /* backoff2*plane spilled to [ESP+0x14] before each subtract. */
                    float scaled2;
                    scaled2 = (float)((long double)backoff2 * (long double)planes[j][0]);
                    clipVelocity[0] = (float)((long double)clipVelocity[0] - (long double)scaled2);
                    scaled2 = (float)((long double)backoff2 * (long double)planes[j][1]);
                    clipVelocity[1] = (float)((long double)clipVelocity[1] - (long double)scaled2);
                    scaled2 = (float)((long double)backoff2 * (long double)planes[j][2]);
                    clipVelocity[2] = (float)((long double)clipVelocity[2] - (long double)scaled2);

                    /* The end velocity is clipped by its OWN dot against
                     * planes[j], not by backoff2 (0x3000eefa..ef12 computes
                     * endClipVelocity . planes[j] into [ESP+0x10]). */
                    {
                        float dotE2 = (float)(
                            ((long double)endClipVelocity[0] * (long double)planes[j][0]
                             + (long double)endClipVelocity[1] * (long double)planes[j][1])
                            + (long double)endClipVelocity[2] * (long double)planes[j][2]);
                        /* 0x3000ef26..ef39: FMUL(neg)/FDIV(non-neg) select. */
                        float backoffE2 = (dotE2 < 0.0f)
                            ? (float)((long double)dotE2 * (long double)PM_OVERCLIP)
                            : (float)((long double)dotE2 / (long double)PM_OVERCLIP);
                        /* backoffE2*plane spilled to [ESP+0x14] per lane. */
                        float scaledE2;
                        scaledE2 = (float)((long double)backoffE2 * (long double)planes[j][0]);
                        endClipVelocity[0] = (float)((long double)endClipVelocity[0] - (long double)scaledE2);
                        scaledE2 = (float)((long double)backoffE2 * (long double)planes[j][1]);
                        endClipVelocity[1] = (float)((long double)endClipVelocity[1] - (long double)scaledE2);
                        scaledE2 = (float)((long double)backoffE2 * (long double)planes[j][2]);
                        endClipVelocity[2] = (float)((long double)endClipVelocity[2] - (long double)scaledE2);
                    }
                }

                /* If the twice-clipped velocity still moves into the FIRST plane,
                 * go to the crossproduct (edge-slide) solution. Compared
                 * UNROUNDED (no float store before the FCOMP) — keep inline. */
                long double firstPlaneDot =
                    ((long double)clipVelocity[0] * (long double)planes[i][0]
                     + (long double)clipVelocity[1] * (long double)planes[i][1])
                    + (long double)clipVelocity[2] * (long double)planes[i][2];
                if (firstPlaneDot >= (long double)0.0f) {
                    continue;
                }

                /* dir = normalize( planes[i] x planes[j] ), then slide velocity
                 * onto that edge: velocity = dir * (dir . velocity). */
                dir[0] = (float)((long double)planes[i][1] * (long double)planes[j][2]
                    - (long double)planes[i][2] * (long double)planes[j][1]);
                dir[1] = (float)((long double)planes[i][2] * (long double)planes[j][0]
                    - (long double)planes[i][0] * (long double)planes[j][2]);
                dir[2] = (float)((long double)planes[i][0] * (long double)planes[j][1]
                    - (long double)planes[i][1] * (long double)planes[j][0]);
                VectorNormalize(dir);                     /* 0x3000efe6 CALL 0x30049700; FSTP ST0 */

                {
                    /* Summed y,z,x (FADDP order 0x3000eff0..f010), one rounding
                     * at the [ESP+0x10] store. */
                    ps = (playerState_t *)move->ps;         /* 0x3000efed */
                    float d = (float)(
                        ((long double)dir[1] * (long double)ps->velocity[1]
                         + (long double)dir[2] * (long double)ps->velocity[2])
                        + (long double)dir[0] * (long double)ps->velocity[0]);
                    clipVelocity[0] = (float)((long double)d * (long double)dir[0]);
                    clipVelocity[1] = (float)((long double)d * (long double)dir[1]);
                    clipVelocity[2] = (float)((long double)d * (long double)dir[2]);

                    {
                        float dE = (float)(
                            ((long double)endVelocity[0] * (long double)dir[0]
                             + (long double)endVelocity[1] * (long double)dir[1])
                            + (long double)endVelocity[2] * (long double)dir[2]);
                        endClipVelocity[0] = (float)((long double)dE * (long double)dir[0]);
                        endClipVelocity[1] = (float)((long double)dE * (long double)dir[1]);
                        endClipVelocity[2] = (float)((long double)dE * (long double)dir[2]);
                    }
                }

                /* Three-plane check: if the edge-slid velocity moves into ANY third
                 * plane, the move is stuck in a corner — stop dead and report blocked. */
                for (k = 0; k < numplanes; k++) {        /* 0x3000f080..f0bf; k = ECX */
                    if (k == j || k == i) {               /* 0x3000f080 CMP ECX,[ESP+0x58](=j); 0x3000f086 CMP ECX,[ESP+0x18](=i) */
                        continue;
                    }
                    /* Compared UNROUNDED (no float store before the FCOMP 0.1
                     * double at 0x3000f0a4); retain the wide intermediate. */
                    WIDE_FLOAT thirdPlaneDot =
                        ((WIDE_FLOAT)clipVelocity[0] * (WIDE_FLOAT)planes[k][0]
                         + (WIDE_FLOAT)clipVelocity[1] * (WIDE_FLOAT)planes[k][1])
                        + (WIDE_FLOAT)clipVelocity[2] * (WIDE_FLOAT)planes[k][2];
                    /* TEST AH,1 again accepts below and unordered. */
                    if (!(thirdPlaneDot >= PM_SLIDEMOVE_INTO_PLANE_EPS)) {
                        ps = (playerState_t *)move->ps;
                        ps->velocity[2] = 0.0f;
                        ps = (playerState_t *)move->ps;
                        ps->velocity[1] = 0.0f;
                        ps = (playerState_t *)move->ps;
                        ps->velocity[0] = 0.0f;
                        return 1;                         /* 0x3000f206: EAX = 1 */
                    }
                }
            }

            /* clipVelocity survived the two-/three-plane resolution: adopt it, then
             * fall through to the next bump (0x3000f0d7..f109 -> f10d). */
            ps = (playerState_t *)move->ps;
            memcpy(&ps->velocity[0], &clipVelocity[0], sizeof(float));
            ps = (playerState_t *)move->ps;
            memcpy(&ps->velocity[1], &clipVelocity[1], sizeof(float));
            ps = (playerState_t *)move->ps;
            memcpy(&ps->velocity[2], &clipVelocity[2], sizeof(float));
            memcpy(&endVelocity[0], &endClipVelocity[0], sizeof(float));
            memcpy(&endVelocity[1], &endClipVelocity[1], sizeof(float));
            memcpy(&endVelocity[2], &endClipVelocity[2], sizeof(float));
        }
    }

    if (gravity) {                                        /* 0x3000f11f MOV [gravity]; TEST; JZ f148 */
        ps = (playerState_t *)move->ps;
        memcpy(&ps->velocity[0], &endVelocity[0], sizeof(float));
        ps = (playerState_t *)move->ps;
        memcpy(&ps->velocity[1], &endVelocity[1], sizeof(float));
        ps = (playerState_t *)move->ps;
        memcpy(&ps->velocity[2], &endVelocity[2], sizeof(float));
    }

    /* If a pmTime was in effect (teleport/knockback lock), don't let the slide have
     * changed velocity: restore the captured primal_velocity. */
    ps = (playerState_t *)move->ps;
    if (ps->pmTime) {                                     /* 0x3000f148 MOV [ps+0x10]; TEST; JZ f16d */
        memcpy(&ps->velocity[0], &primal_velocity[0], sizeof(float));
        ps = (playerState_t *)move->ps;
        memcpy(&ps->velocity[1], &primal_velocity[1], sizeof(float));
        ps = (playerState_t *)move->ps;
        memcpy(&ps->velocity[2], &primal_velocity[2], sizeof(float));
    }

    /* Return qtrue when at least one bump ran (numplanes-loop ran and bumpcount was
     * advanced) — i.e. the move was blocked. The machine returns SETNZ(bumpcount):
     * bumpcount != 0 => blocked. */
    return (bumpcount != 0) ? 1 : 0;                      /* 0x3000f16d..f178: SETNZ AL on [ESP+0x5c] */
}

// Source: uo_cgame_mp_x86.dll 0x3000f220..0x3000fa05
// Evidence: cgame_mp/mcode/uo_cgame_mp_x86/FUN_3000f220_3000fa05.mcode
//
// PM_StepSlideMove(qboolean gravity)
//
// The Quake3/CoD pmove step-and-slide mover. It runs the collision slide core
// PM_SlideMove(gravity) once from the current player-state origin/velocity; when
// that slide was blocked (returned nonzero) or the player is on a step-eligible
// ground plane, it "steps up over the obstacle, slides, then steps back down" so
// the player walks up stairs/ledges instead of bumping into them. On a used step
// it emits an EV_STEP_VIEW predictable event with the clamped step height and re-runs
// the footstep classifier, and it smooths the interpolated view velocity so the
// step reads cleanly. Verbose movement diagnostics are gated on the pmove debug
// flag (pm->debugMove, +0x38).
//
// The mechanical header name `G_PlayerTurretPositionAndBlend` is a SERVER-style
// name assigned purely by size match (win 0x7e5 vs matched 0x7f8) and is REJECTED
// per the no-size-naming rule. The behavior is proven by the debug .rdata strings
// pushed to Com_Printf, all in the movement.c "step" cluster:
//   "%i:stepped %2i\n" (0x30074ab8), "%i:jump step %2i\n" (0x30074ac8),
//   "%i:didn't use step results\n" (0x30074b30),
//   "%i:didn't use jump step results because it went too high\n" (0x30074b4c),
//   "%i:did down step after not using step results\n" (0x30074b00),
//   "%i:adjusted jump vel: %.1f -> %.1f\n" (0x30074adc),
//   "%i:not enough step room\n" (0x30074b88);
// the "%i" is c_pmove (0x30134cd0). The callee graph is the pmove core:
// PM_SlideMove (0x3000e930), PM_trace (0x30008280), PM_ClipVelocity (0x30008390),
// PM_VerifyPronePosition (0x3000e840 — whose own .mcode names THIS function as its
// sole caller), PM_FootstepEvent (0x3000b950), PM_ShouldMakeFootsteps (0x3000bb60),
// Q_rint (0x3006be3c) and a round-to-nearest helper (0x3000e810). PM_StepSlideMove
// is the standard Quake3 symbol for this wrapper.
//
// ABI: one arg `gravity` at [ESP+0x9c] after the four register pushes; the
// function ends in a plain RET so the caller cleans the one dword (cdecl). All
// state is the global pmove context pm (0x30539850); pm->ps
// is the playerState the movers read/write (globals.h DIVERGENCE note: `anim` is
// really pmove->ps). Register args to the register-ABI callees are re-derived from
// this function's own bytes at each call site, not from the callees' decls.

/* PM_SlideMove core (0x3000e930): runs the pmove slide/clip iteration for this
 * substep and returns nonzero when the move was blocked (bumped an obstacle), zero
 * when it slid freely. Single arg `gravity` pushed by this caller (0x3000f2ac PUSH
 * EBP; 0x3000f2cb ADD ESP,0x4 cleans it). Provisional caller-observed decl;
 * superseded by its own .mcode. NOTE: 0x3000e930's own .mcode carries the
 * size-guess "PM_StepSlideMove" — that is the OUTER wrapper's name applied to the
 * inner core; the actual step wrapper is THIS function (0x3000f220). Kept as
 * PM_SlideMove by role; arity/return UNPROVEN beyond the (gravity)->int32 seen here.
 * The shared declaration is in bg_pmove.h. */

/* PM_ClipVelocity (0x30008390): out = in - overbounce * (in . normal) * normal, the
 * standard Quake3 velocity-vs-plane clip. Register ABI proven at both call sites in
 * this function: EDX = in (const vec3 *), ECX = normal (const vec3 *), ESI = out
 * (vec3 *); the overbounce scalar is one pushed stack dword (0x3f8020c5 == 1.001f,
 * the Quake3 OVERCLIP). The sign of the dot selects FMUL vs FDIV of the scale.
 * Its recovered declaration is centralized in client_recovered.h. */

/* PM_RoundStepHeight (0x3000e810): rounds a float (one pushed stack dword) to the
 * nearest int (FLD arg; FADD double 9.3e-10 @0x3007be50; FISTP under the default
 * round-to-nearest control word) and returns it in EAX. Quantizes the measured
 * step delta into integer units. Exact source symbol unresolved; named by role.
 * The recovered shared name is Script_RoundToNearestInt, declared centrally. */

/* EV_STEP_VIEW (145, 0x91): predictable player-state event pushed into the event ring
 * (events[eventIndex & 3] = 145) with the biased step height as its parm on a used
 * step. The ring here is 4 dword-wide entries (mask 0x3, stride 4) at events (+0x8c)
 * / eventParms (+0x9c) of playerState_t. The shared event-name tables confirm
 * the EV_STEP_VIEW identity. */

/* Signed step-delta clamp: the rounded integer step delta is clamped into
 * [PM_STEP_MIN, PM_STEP_MAX] before being biased by +0x80 and stored as the
 * unsigned EV_STEP_VIEW parm byte (128 == neutral midpoint). */
enum {
    PM_STEP_MIN = -16,   /* 0xfffffff0 */
    PM_STEP_MAX =  24    /* 0x18 */
};
#define PM_STEP_PARM_BIAS 128u

/* Step-down probe: when trace.entityNum (+0x28, uint16) is below this the step
 * landed on genuine low-numbered geometry, so the un-stepped slide result is
 * committed instead of the stepped one. The immediate is 64. Role-named; exact meaning
 * unresolved. */
enum { PM_STEP_DOWN_ENTITY_LIMIT = 64 };

/* Small movement-noise epsilon (0x3007bd94 == 0.001f): the planar step move must
 * cover at least this squared distance for the step to count. */
#define PM_STEP_MIN_MOVE_SQ 0.001f

void PM_StepSlideMove(int32_t gravity)
{
    pmove_t *move = pm;
    playerState_t *ps = move->ps;
    uint32_t flags = ps->playerStateFlags;                    /* [ps+0xc] */

    int32_t airStepFlag  = 0;   /* [ESP+0x20]: this move was a jump/air step */
    int32_t groundStep;         /* [ESP+0x24]: this move is ground-step eligible */

    vec3_t  startOrigin;        /* [ESP+0x28..0x30] */
    vec3_t  startVelocity;      /* [ESP+0x5c..0x64] */
    vec3_t  afterOrigin;        /* [ESP+0x34..0x3c] */
    vec3_t  afterVelocity;      /* [ESP+0x4c..0x54] */
    float   stepUpReach;        /* [ESP+0x1c]: candidate step-up height */
    float   stepHeight = 0.0f;  /* [ESP+0x18]: achieved step-up height (0 == no step) */
    trace_t trace;              /* [ESP+0x70..] */

    // 0x3000f23d..0x3000f28c: pick the ground-step flag and clear a stale JUMPED
    // timer / take-off height where appropriate.
    if (flags & PMF_LADDER) {
        ps->playerStateFlags = flags & ~(uint32_t)PMF_WALLJUMP;   // 0x3000f254/f256
        ps->jumpOriginZ = 0.0f;                               // 0x3000f25f
        groundStep = 0;
    } else if (pml.groundPlane != 0) {
        groundStep = 1;                                       // 0x3000f26c
    } else {
        groundStep = 0;                                       // 0x3000f279
        if ((flags & PMF_WALLJUMP) && ps->pmTime != 0) {
            ps->playerStateFlags &= ~(uint32_t)PMF_WALLJUMP;      // 0x3000f284
            ps->jumpOriginZ = 0.0f;                           // 0x3000f289
        }
    }

    // 0x3000f28c..0x3000f2b5: snapshot the pre-move origin and velocity.
    ps = move->ps;
    memcpy(&startOrigin[0], &ps->psOrigin[0], sizeof(float));
    memcpy(&startOrigin[1], &ps->psOrigin[1], sizeof(float));
    memcpy(&startOrigin[2], &ps->psOrigin[2], sizeof(float));
    memcpy(&startVelocity[0], &ps->velocity[0], sizeof(float));
    memcpy(&startVelocity[1], &ps->velocity[1], sizeof(float));
    memcpy(&startVelocity[2], &ps->velocity[2], sizeof(float));

    // 0x3000f2b9: run the slide core; its result is the blocked/free flag.
    int32_t slideBlocked = PM_SlideMove(gravity);

    move = pm;                                    /* 0x3000f2be */
    ps = move->ps;
    flags = ps->playerStateFlags;                                        // 0x3000f2c8

    // 0x3000f2d0..0x3000f2da: step-up reach depends on flags bit 0x1.
    stepUpReach = (flags & PMF_PRONE) ? 10.0f : 18.0f;

    // 0x3000f2e2: on-ground path skips the airborne step-eligibility gate.
    if (ps->groundEntityNum == ENTITYNUM_NONE) {
        // 0x3000f2ef: clear a stale JUMPED timer if it ran out.
        if ((flags & PMF_WALLJUMP) && ps->pmTime != 0) {
            ps->playerStateFlags &= ~(uint32_t)PMF_WALLJUMP;
            ps->jumpOriginZ = 0.0f;
        }

        int handled = 0;
        if (slideBlocked != 0) {                              // 0x3000f301 (JZ f36f)
            ps = move->ps;
            flags = ps->playerStateFlags;                                // 0x3000f307
            if (flags & PMF_WALLJUMP) {                // 0x3000f30a (JZ f36f)
                // 0x3000f30f: ceiling on the take-off height for a jump-step.
                float ceiling = (float)((long double)ps->jumpOriginZ
                    + (long double)39.0f);
                // 0x3000f320 FCOMP; TEST AH,5; JP f36f: startOrigin.z >= ceiling
                // takes the low-check path (no jump step here).
                if (startOrigin[2] < ceiling) {
                    stepUpReach = 18.0f;                      // 0x3000f32f
                    // 0x3000f33d FCOMP ceiling; TEST AH,0x41 (<=); JNZ f365.
                    long double startPlusStep = (long double)startOrigin[2]
                        + (long double)18.0f;
                    /* TEST AH,0x41 takes less, equal, and unordered. */
                    if (!(startPlusStep > (long double)ceiling)) {
                        airStepFlag = 1;                      // 0x3000f365
                    } else {
                        // 0x3000f348: room left under the ceiling.
                        long double stepUpReachRaw =
                            (long double)ceiling -
                            (long double)startOrigin[2];
                        stepUpReach = (float)stepUpReachRaw; // FST [ESP+0x1c] keeps st0
                        // 0x3000f354 FCOMP 1.0; TEST AH,5; JNP f9fa: room < 1.0
                        // means there is not enough clean step room -> bail.
                        // The FCOMP compares the UNROUNDED difference retained
                        // on the x87 stack (FST, not FSTP), so the compare
                        // recomputes it instead of reading the rounded local.
                        if (stepUpReachRaw < 1.0L) {
                            return;                           // -> 0x3000f9fa
                        }
                        airStepFlag = 1;                      // 0x3000f365
                    }
                    handled = 1;                              // 0x3000f36d JMP f38f
                }
                // startOrigin.z >= ceiling falls through to the low-check (f36f).
            }
        }
        if (!handled) {
            // 0x3000f36f: only continue if BIT4 is set and moving upward.
            ps = move->ps;
            if ((ps->playerStateFlags & PMF_LADDER) == 0) {
                return;                                       // 0x3000f375
            }
            if (!(ps->velocity[2] > 0.0f)) {
                return;                                       // 0x3000f386
            }
        }
    }

    // 0x3000f38f..0x3000f3cf: capture the post-slide origin/velocity and planar delta.
    ps = move->ps;
    memcpy(&afterOrigin[0], &ps->psOrigin[0], sizeof(float));
    memcpy(&afterOrigin[1], &ps->psOrigin[1], sizeof(float));
    memcpy(&afterOrigin[2], &ps->psOrigin[2], sizeof(float));
    memcpy(&afterVelocity[0], &ps->velocity[0], sizeof(float));
    memcpy(&afterVelocity[1], &ps->velocity[1], sizeof(float));
    memcpy(&afterVelocity[2], &ps->velocity[2], sizeof(float));
    float moveDeltaX = (float)((long double)afterOrigin[0]
        - (long double)startOrigin[0]);
    float moveDeltaY = (float)((long double)afterOrigin[1]
        - (long double)startOrigin[1]);

    // stepHeight ([ESP+0x18]) is 0.0 from entry (0x3000f241) unless the step-up
    // path below sets it.
    if (slideBlocked != 0) {                                  // 0x3000f3d3 (JZ f4cf)
        // 0x3000f3d9..0x3000f423: STEP-UP probe from the pre-move origin up to
        // origin + (stepUpReach + 1) in Z (X/Y unchanged).
        vec3_t stepUpDest;
        memcpy(&stepUpDest[0], &startOrigin[0], sizeof(float));
        memcpy(&stepUpDest[1], &startOrigin[1], sizeof(float));
        stepUpDest[2] = (float)((long double)startOrigin[2]
            + ((long double)stepUpReach + (long double)1.0f));

        ps = move->ps;
        PM_trace(&trace, startOrigin, move->mins, move->maxs, stepUpDest,
                 (int32_t)ps->psClientNum, move->traceMask);

        // 0x3000f428..0x3000f44e: achieved step height. The FST at 0x3000f43f
        // keeps the UNROUNDED chain on st0 for the FCOMP 1.0 at 0x3000f443, so
        // the compare recomputes the expression instead of reading the rounded
        // local.
        long double stepHeightRaw =
            ((long double)stepUpReach + 1.0L) *
                (long double)trace.fraction -
            1.0L;
        stepHeight = (float)stepHeightRaw;
        /* JP also selects this path for an unordered x87 comparison. */
        if (!(stepHeightRaw < 1.0L)) {
            // 0x3000f481: use the step-up. Move to the raised position (X/Y from
            // stepUpDest == startOrigin X/Y, Z = startOrigin.z + stepHeight),
            // restore the pre-slide velocity, and re-run the slide from there.
            move = pm;                             /* 0x3000f481 */
            ps = move->ps;
            memcpy(&ps->psOrigin[0], &stepUpDest[0], sizeof(float));
            ps = move->ps;
            memcpy(&ps->psOrigin[1], &stepUpDest[1], sizeof(float));
            ps = move->ps;
            ps->psOrigin[2] = (float)((long double)startOrigin[2]
                + (long double)stepHeight);
            ps = move->ps;
            memcpy(&ps->velocity[0], &startVelocity[0], sizeof(float));
            ps = move->ps;
            memcpy(&ps->velocity[1], &startVelocity[1], sizeof(float));
            ps = move->ps;
            memcpy(&ps->velocity[2], &startVelocity[2], sizeof(float));
            (void)PM_SlideMove(gravity);                      // 0x3000f4c1
            move = pm;                               /* 0x3000f4c6 */
        } else {
            // 0x3000f450: not enough gain — optional diagnostic and abandon step.
            move = pm;                               /* 0x3000f450 */
            if (move->debugMove != 0) {
                Com_Printf("%i:not enough step room\n", c_pmove);
                move = pm;                           /* 0x3000f46e */
            }
            stepHeight = 0.0f;                                // 0x3000f477 ([ESP+0x18]=0)
        }
    }

    // 0x3000f4cf: STEP-DOWN phase. groundStep selects whether to run the down trace
    // unconditionally or first check that the slide actually left the ground.
    ps = move->ps;
    if (groundStep == 0) {
        // 0x3000f4d7: if the achieved step height (var18) is ~0.0, skip the down
        // trace entirely and go to the final delta/step-event phase.
        if (stepHeight == 0.0f) {                             // FUCOMPP; TEST AH,0x44; JNP
            goto step_finish;                                 // 0x3000f610
        }
    }

    // 0x3000f4ee..0x3000f54e: trace straight down from the post-slide origin by the
    // step height, offset the X/Y down-target by 9.0 when this is a ground step.
    {
        vec3_t downTarget;                                    /* [ESP+0x40..0x48] */
        ps = move->ps;
        memcpy(&downTarget[0], &ps->psOrigin[0], sizeof(float));
        ps = move->ps;
        memcpy(&downTarget[1], &ps->psOrigin[1], sizeof(float));
        ps = move->ps;
        downTarget[2] = (float)((long double)ps->psOrigin[2]
            - (long double)stepHeight);
        if (groundStep != 0) {
            downTarget[2] = (float)((long double)downTarget[2]
                - (long double)9.0f);
        }

        ps = move->ps;
        PM_trace(&trace, ps->psOrigin, move->mins, move->maxs, downTarget,
                 (int32_t)ps->psClientNum, move->traceMask);

        // 0x3000f551: if the down trace hit a low-numbered entity, discard the
        // stepped result and commit the un-stepped post-slide origin/velocity.
        if ((uint16_t)trace.entityNum < PM_STEP_DOWN_ENTITY_LIMIT) {
            move = pm;                               /* 0x3000f55c */
            ps = move->ps;
            memcpy(&ps->psOrigin[0], &afterOrigin[0], sizeof(float));
            ps = move->ps;
            memcpy(&ps->psOrigin[1], &afterOrigin[1], sizeof(float));
            ps = move->ps;
            memcpy(&ps->psOrigin[2], &afterOrigin[2], sizeof(float));
            ps = move->ps;
            memcpy(&ps->velocity[0], &afterVelocity[0], sizeof(float));
            ps = move->ps;
            memcpy(&ps->velocity[1], &afterVelocity[1], sizeof(float));
            // 0x3000f591/f597: the POP EDI at 0x3000f58b shifts ESP by 4 before
            // the second [ESP+0x50] read, so it addresses var54 — a clean
            // velocity[2] = afterVelocity[2] copy.
            ps = move->ps;
            memcpy(&ps->velocity[2], &afterVelocity[2], sizeof(float));
            return;                                           // 0x3000f5a1 RET
        }

        // 0x3000f5a2: FLD trace.fraction; FCOMP 1.0; TEST AH,5; JP f5f1.
        // fraction < 1.0 (trace was blocked before the full step-down): snap to the
        // traced endpoint and clip velocity against the OVERCLIP plane. fraction >=
        // 1.0 (trace reached the far end): when a real step was taken, back the
        // origin down by the step height.
        {
            long double downFraction = (long double)trace.fraction;
            move = pm;                               /* 0x3000f5a6 */
            if (downFraction < (long double)1.0f) {
                ps = move->ps;
                memcpy(&ps->psOrigin[0], &trace.endpos[0], sizeof(float));
                ps = move->ps;
                memcpy(&ps->psOrigin[1], &trace.endpos[1], sizeof(float));
                ps = move->ps;
                memcpy(&ps->psOrigin[2], &trace.endpos[2], sizeof(float));
                ps = move->ps;
                PM_ClipVelocity(ps->velocity, trace.normal, ps->velocity, 1.001f);
            } else if (stepHeight != 0.0f) {                  // 0x3000f5f1..f602
                ps = move->ps;
                ps->psOrigin[2] = (float)((long double)ps->psOrigin[2]
                    - (long double)stepHeight);
            }
        }
    }

step_finish:
    // 0x3000f610..0x3000f647: require the planar step move to exceed the noise
    // epsilon (deltaX*vel.x + deltaY*vel.y + 0.001 vs the trace-delta dot) before
    // treating the step as real.
    ps = move->ps;
    {
        // lhs = dx*vel.x + dy*vel.y ; rhs = moveDeltaX*vel.x + moveDeltaY*vel.y + 0.001
        // 0x3000f612..f640: dx/dy and both dot sums stay ENTIRELY on the x87
        // stack (no float store anywhere), and the FCOMPP compares the two
        // UNROUNDED 80-bit sums — so the whole test must be one expression.
        // FCOMPP compares ST0=rhs vs ST1=lhs; TEST AH,0x1 (C0); JZ f66c.
        // C0 is set iff rhs < lhs, so JZ (C0 clear => rhs >= lhs => lhs <= rhs) takes
        // the commit-plain-slide path at f66c. The step is "kept" (fall-through to
        // f649) only when it moved MORE along the velocity than the plain slide:
        // lhs > rhs.
        long double keptStepDot =
            ((long double)ps->psOrigin[0] - (long double)startOrigin[0])
                * (long double)ps->velocity[0]
            + ((long double)ps->psOrigin[1] - (long double)startOrigin[1])
                * (long double)ps->velocity[1];
        long double plainSlideDot =
            ((long double)moveDeltaX * (long double)ps->velocity[0]
             + (long double)moveDeltaY * (long double)ps->velocity[1])
            + (long double)PM_STEP_MIN_MOVE_SQ;
        /* C0 is set for rhs < lhs and for unordered; both keep the step. */
        if (!(keptStepDot <= plainSlideDot)) {
            // 0x3000f649: air-step case — validate against the take-off ceiling.
            if (airStepFlag != 0) {
                // 0x3000f655..f65e: the sum is compared UNROUNDED (FLD/FADD
                // straight into FCOMP psOrigin.z, no store); TEST AH,0x41; JP
                // f7a4: JP taken when (jumpOriginZ+39) > psOrigin.z.
                long double jumpCeiling = (long double)ps->jumpOriginZ
                    + (long double)39.0f;
                /* JP selects greater and unordered outcomes. */
                if (!(jumpCeiling <= (long double)ps->psOrigin[2])) {
                    goto after_commit;                        // 0x3000f7a4
                }
            } else {
                goto no_step_used;                            // 0x3000f64f JZ f841
            }
        }
    }

    // 0x3000f66c..0x3000f69d: commit the un-stepped (post-slide) origin/velocity —
    // the step was rejected as not producing a useful move.
    memcpy(&ps->psOrigin[0], &afterOrigin[0], sizeof(float));
    ps = move->ps;
    memcpy(&ps->psOrigin[1], &afterOrigin[1], sizeof(float));
    ps = move->ps;
    memcpy(&ps->psOrigin[2], &afterOrigin[2], sizeof(float));
    ps = move->ps;
    memcpy(&ps->velocity[0], &afterVelocity[0], sizeof(float));
    ps = move->ps;
    memcpy(&ps->velocity[1], &afterVelocity[1], sizeof(float));
    ps = move->ps;
    memcpy(&ps->velocity[2], &afterVelocity[2], sizeof(float));

    // 0x3000f6a0: debug-print the discarded step results.
    if (move->debugMove > 1) {                                 // CMP [EDI+0x38],1; JLE
        if (airStepFlag != 0) {
            Com_Printf("%i:didn't use jump step results because it went too high\n",
                       c_pmove);
        } else {
            Com_Printf("%i:didn't use step results\n", c_pmove);
        }
        move = pm;                                  /* 0x3000f6cd */
    }

    // 0x3000f6d6: on a ground step, try a corrective down step even after rejecting.
    if (groundStep != 0) {
        vec3_t downTarget2;                                   /* [ESP+0x40..0x48] */
        ps = move->ps;
        memcpy(&downTarget2[0], &ps->psOrigin[0], sizeof(float));
        ps = move->ps;
        memcpy(&downTarget2[1], &ps->psOrigin[1], sizeof(float));
        ps = move->ps;
        downTarget2[2] = (float)((long double)ps->psOrigin[2]
            - (long double)9.0f);

        ps = move->ps;
        PM_trace(&trace, ps->psOrigin, move->mins, move->maxs, downTarget2,
                 (int32_t)ps->psClientNum, move->traceMask);

        // 0x3000f72f: FLD trace.fraction; FCOMP 1.0; TEST AH,5; JP f7a4.
        // Only apply the corrective down step when the trace was blocked before the
        // full drop (fraction < 1.0).
        {
            long double correctiveFraction = (long double)trace.fraction;
            move = pm;                              /* 0x3000f73c */
            if (correctiveFraction < (long double)1.0f) {
                ps = move->ps;
                memcpy(&ps->psOrigin[0], &trace.endpos[0], sizeof(float));
                ps = move->ps;
                memcpy(&ps->psOrigin[1], &trace.endpos[1], sizeof(float));
                ps = move->ps;
                memcpy(&ps->psOrigin[2], &trace.endpos[2], sizeof(float));
                ps = move->ps;
                PM_ClipVelocity(ps->velocity, trace.normal, ps->velocity, 1.001f);
                if (move->debugMove > 1) {                      // 0x3000f785 CMP EAX,1; JLE
                    Com_Printf("%i:did down step after not using step results\n",
                               c_pmove);
                    move = pm;                       /* 0x3000f79b */
                }
            }
        }
    }

after_commit:
    // 0x3000f7a4: air-step landing-velocity clamp for the used jump step.
    if (airStepFlag != 0) {
        ps = move->ps;
        // 0x3000f7b2: (psOrigin.z - afterOrigin.z) <= 0 => nothing to clamp.
        // FCOMP 0.0; TEST AH,0x41 (<=); JNZ f841.
        long double raisedBy = (long double)ps->psOrigin[2]
            - (long double)afterOrigin[2];
        if (!(raisedBy > (long double)0.0f)) {
            goto no_step_used;
        }
        // 0x3000f7c6: slack = jumpOriginZ + 39 - psOrigin.z, compared vs 0.1.
        // The slack chain is NEVER stored: FLD/FADD/FSUB straight into FCOM
        // 0.1f, and the retained st0 feeds the sqrt chain — so both uses
        // recompute the expression (bit-identical, same memory operands).
        long double slack = ((long double)ps->jumpOriginZ
            + (long double)39.0f) - (long double)ps->psOrigin[2];
        if (slack < (long double)0.1f) { // FCOM 0.1; TEST AH,0x5; JP f7ea (>=/unordered)
            // 0x3000f7df/f7e1: too little slack -> zero the vertical velocity.
            ps->velocity[2] = 0.0f;
        } else {
            // 0x3000f7ea..f7f5: launchZ = sqrt(gravity * 2 * slack); the FSQRT
            // runs on the raw 80-bit product (bare FILD gravity, FADD ST0,ST0
            // on the unrounded slack), rounded only at the FSTP [ESP+0x10].
            float launchZ = (float)sqrtl(
                (long double)ps->gravity * (slack + slack));
            // 0x3000f7f9: only clamp when the current vertical velocity exceeds it.
            // FCOMP launchZ; TEST AH,0x41 (<=); JNZ f841.
            if (!(ps->velocity[2] > launchZ)) {
                goto no_step_used;
            }
            if (move->debugMove != 0) {
                // 0x3000f80e..f82a: "%.1f -> %.1f" = (current vel.z, launchZ).
                Com_Printf("%i:adjusted jump vel: %.1f -> %.1f\n",
                           c_pmove, (double)ps->velocity[2], (double)launchZ);
                move = pm;                           /* 0x3000f82f */
            }
            ps = move->ps;
            memcpy(&ps->velocity[2], &launchZ, sizeof(float));
        }
    }

no_step_used:
    // 0x3000f841: the EV_STEP_VIEW predictable-event + footstep phase, only for a used
    // ground step where the player state is not in a high pmType.
    if (groundStep == 0) {
        return;                                               // 0x3000f847 JZ f9fa
    }
    ps = move->ps;
    if (ps->pmType >= PM_TYPE_DEAD) {
        return;
    }

    // 0x3000f859..0x3000f868: verify the stepped prone position is valid.
    if (PM_VerifyPronePosition(startOrigin, startVelocity) == 0) {
        return;                                               // 0x3000f86d
    }

    // 0x3000f873..0x3000f893: measured vertical step = psOrigin.z - afterOrigin.z.
    // Ignore if its magnitude is under 0.5 (a double compare). The FST at
    // 0x3000f882 keeps the UNROUNDED difference on st0 for the FABS/FCOMP, so
    // the magnitude test recomputes the difference (|d| <= 0.5 written as two
    // signed compares) instead of reading back the rounded local.
    move = pm;                                       /* 0x3000f873 */
    ps = move->ps;
    long double stepZRaw =
        (long double)ps->psOrigin[2] - (long double)afterOrigin[2];
    float stepZ = (float)stepZRaw;                            // FST [ESP+0x10]
    if (!(stepZRaw > 0.5L || stepZRaw < -0.5L)) {
        return;                                               // 0x3000f893
    }

    // 0x3000f899..0x3000f8aa: quantize the step to an integer; nothing to do if 0.
    int32_t stepInt = Script_RoundToNearestInt(stepZ);
    if (stepInt == 0) {
        return;
    }

    // 0x3000f8b0..0x3000f8e4: optional "stepped %2i" / "jump step %2i" diagnostics.
    if (move->debugMove != 0) {
        if (airStepFlag != 0) {
            Com_Printf("%i:jump step %2i\n", c_pmove, stepInt);
        } else {
            Com_Printf("%i:stepped %2i\n", c_pmove, stepInt);
        }
        move = pm;                                   /* 0x3000f8de */
    }

    // 0x3000f8e7..0x3000f8f8: clamp the step delta into [-16, +24].
    if (stepInt < PM_STEP_MIN) {
        stepInt = PM_STEP_MIN;
    } else if (stepInt > PM_STEP_MAX) {
        stepInt = PM_STEP_MAX;
    }

    // 0x3000f8fd..0x3000f92c: push EV_STEP_VIEW with the biased delta into the event ring.
    ps = move->ps;
    ps->events[ps->eventIndex & 3] = EV_STEP_VIEW;            // dword store 0x91
    ps->eventParms[ps->eventIndex & 3] =
        (uint8_t)((uint32_t)(stepInt + (int32_t)PM_STEP_PARM_BIAS) & 0xffu);
    ps->eventIndex = coduo_int32_from_bits((uint32_t)ps->eventIndex + 1u);

    // 0x3000f932..0x3000f984: smooth the interpolated view height fraction. The
    // origin z-shift this frame drives a 0.8/0.2 blend that scales all three
    // velocity components (a step-smoothing kick).
    ps = move->ps;
    {
        float shift = (float)((long double)ps->psOrigin[2]
            - (long double)startOrigin[2]);
        // AND [ESP+0x10],0x7fffffff => |shift|
        uint32_t bits;
        float absShift;

        memcpy(&bits, &shift, sizeof(bits));
        bits &= UINT32_C(0x7fffffff);
        memcpy(&absShift, &bits, sizeof(absShift));

        // scale = (1.0 - |shift|/stepUpReach) * 0.8 + ~0.2. The addend at
        // 0x3000f967 (FADD [0x3007c230]) is 0x3e4ccccc — one ULP BELOW 0.2f
        // (0x3e4ccccd); it is exactly float(1.0f - 0.8f). 0.19999999f encodes
        // to that bit pattern.
        // The scale chain is NEVER stored (0x3000f953..f984). FLD ST0 duplicates
        // one retained x87 value for the first two lanes; the third multiply
        // consumes the original value.
        long double scale = (((long double)1.0f
            - (long double)absShift / (long double)stepUpReach)
            * (long double)0.8f) + (long double)0.19999999f;
        ps->velocity[0] = (float)(scale * (long double)ps->velocity[0]);
        ps = move->ps;
        ps->velocity[1] = (float)(scale * (long double)ps->velocity[1]);
        ps = move->ps;
        ps->velocity[2] = (float)(scale * (long double)ps->velocity[2]);
    }

    // 0x3000f987..0x3000f993: |stepInt| must exceed 3 to bother with a footstep.
    int32_t stepAbs = stepInt < 0 ? -stepInt : stepInt;       // CDQ/XOR/SUB idiom
    if (stepAbs <= 3) {
        return;
    }

    // 0x3000f995..0x3000f9a7: skip when airborne or when the animation time did not
    // advance to a fresh unlocked frame.
    ps = move->ps;
    if (ps->groundEntityNum == ENTITYNUM_NONE) {
        return;                                               // 0x3000f99e
    }
    if (PM_ShouldMakeFootsteps() == 0) {
        return;                                               // 0x3000f9a7
    }

    // 0x3000f9a9..0x3000f9f5: derive a bob-cycle advance from |stepInt|/2 (capped
    // at 4), quantize it, and fire the footstep event.
    int32_t half = stepAbs / 2;                               // (stepAbs - sign)>>1 (rounds toward 0)
    if (half > 4) {
        half = 4;
    }
    int32_t prevBobCycle = ps->bobCycle;                      // [EBX+0x8] -> ESI
    // _ftol2 truncates the live x87 sum, then the result is masked to a byte.
    long double bobStep = ((long double)half * (long double)1.25f
        + (long double)7.0f) + (long double)prevBobCycle;
    int32_t newBob = coduo_fp_to_i32_extended(bobStep) & 255;
    ps->bobCycle = newBob;

    ps = move->ps;
    // 0x3000f9eb..0x3000f9f5: the register ABI supplies previous bob cycle,
    // forced emission, and current bob cycle; the shared source declaration
    // uses the canonical old/current/shouldMake order.
    PM_FootstepEvent(prevBobCycle, ps->bobCycle, 1);
}
