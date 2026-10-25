#include "bg_pmove.h"

#include "compat/coduo_fp_conversion.h"
#include "compat/coduo_int32_bits.h"

/*
 * Complete pmove foliage-sound subsystem.  The Windows cgame/game bodies are
 * instruction-identical apart from relocations and dependency addresses:
 *
 *   uo_cgame_mp_x86.dll 0x3000c110
 *   uo_game_mp_x86.dll  0x2000bed0
 *
 * Linux game retains the same predicates, trace, event, and timer update at
 * RVA 0x0002a550.  Its fraction local has one genuine additional binary32
 * store/reload before the interval interpolation; see the platform discrepancy
 * record.  That boundary is the only behavior selection in this shared body.
 */

#define PM_FOLIAGE_BOX_XY_SCALE 0.75f
#define PM_FOLIAGE_BOX_Z_SCALE 0.9f
#define PM_FOLIAGE_FRACTION_MAX 1.0f

void PM_FoliageSounds(void)
{
    const float speed = pm->horizontalSpeed;
    const int32_t slowInterval = bg_foliagesnd_slowinterval.integer;
    const int32_t fastInterval = bg_foliagesnd_fastinterval.integer;
    const int32_t speedRange = coduo_int32_from_bits(
        (uint32_t)bg_foliagesnd_maxspeed.integer -
        (uint32_t)bg_foliagesnd_minspeed.integer);
    const int32_t intervalRange = coduo_int32_from_bits(
        (uint32_t)fastInterval - (uint32_t)slowInterval);
    int32_t interval;
    vec3_t mins;
    vec3_t maxs;
    trace_t trace;

    if ((long double)speed <
        (long double)bg_foliagesnd_minspeed.integer) {
        if (coduo_int32_from_bits(
                (uint32_t)pm->ps->foliageSoundTime +
                (uint32_t)bg_foliagesnd_resetinterval.integer) <
            pm->command.commandTime) {
            pm->ps->foliageSoundTime = 0;
        }
        return;
    }

    {
        long double fraction =
            ((long double)speed -
             (long double)bg_foliagesnd_minspeed.integer) /
            (long double)speedRange;

        if (fraction > (long double)PM_FOLIAGE_FRACTION_MAX) {
            fraction = (long double)PM_FOLIAGE_FRACTION_MAX;
        }
        interval = coduo_fp_to_i32_extended(
            (long double)intervalRange * fraction +
            (long double)slowInterval);
    }

    if (coduo_int32_from_bits((uint32_t)pm->ps->foliageSoundTime +
                              (uint32_t)interval) >=
        pm->command.commandTime) {
        return;
    }

    mins[0] = pm->mins[0] * PM_FOLIAGE_BOX_XY_SCALE;
    mins[1] = pm->mins[1] * PM_FOLIAGE_BOX_XY_SCALE;
    mins[2] = pm->mins[2] * PM_FOLIAGE_BOX_XY_SCALE;
    maxs[0] = pm->maxs[0] * PM_FOLIAGE_BOX_XY_SCALE;
    maxs[1] = pm->maxs[1] * PM_FOLIAGE_BOX_XY_SCALE;
    maxs[2] = pm->maxs[2] * PM_FOLIAGE_BOX_Z_SCALE;

    PM_trace(&trace, pm->ps->psOrigin, mins, maxs, pm->ps->psOrigin,
             pm->ps->psClientNum, CONTENTS_FOLIAGE);
    if (trace.startsolid == 0) {
        return;
    }

    PM_AddEvent(EV_FOLIAGE_SOUND);
    pm->ps->foliageSoundTime = pm->command.commandTime;
}

#undef PM_FOLIAGE_BOX_XY_SCALE
#undef PM_FOLIAGE_BOX_Z_SCALE
#undef PM_FOLIAGE_FRACTION_MAX
