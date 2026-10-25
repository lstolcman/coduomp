#include "bg_pmove.h"

#include "bg_player_state.h"
#include "compat/coduo_int32_bits.h"
#include "compat/coduo_x87_eval.h"

#include <stddef.h>
#include <stdint.h>


/*
 * The complete PM trace/touch primitive cluster is shared by Windows cgame,
 * Windows game, and Linux game.  The two Windows blocks are instruction-
 * identical apart from the pm global address:
 *
 *   uo_cgame_mp_x86.dll  0x30008280..0x300083d7
 *   uo_game_mp_x86.dll   0x20008030..0x20008187
 *
 * Linux retains the same operations at RVAs 0x00023174..0x000233e0.  Its
 * PM_ClipVelocity alone has a different floating-point realization. The
 * maintained body below follows the Windows arithmetic graph.
 */

void PM_trace(trace_t *results, const vec3_t start, const vec3_t mins,
              const vec3_t maxs, const vec3_t end, int32_t passEntityNum,
              int32_t traceType)
{
    pm->trace(results, start, mins, maxs, end, passEntityNum, traceType);

    if (results->startsolid != 0 &&
        (results->contents & (int32_t)CONTENTS_BODY) != 0) {
        PM_AddTouchEnt(results->entityNum);
        pm->traceMask = coduo_int32_from_bits(
            (uint32_t)pm->traceMask & ~CONTENTS_BODY);
        traceType = coduo_int32_from_bits(
            (uint32_t)traceType & ~CONTENTS_BODY);
        pm->trace(results, start, mins, maxs, end, passEntityNum, traceType);
    }
}

void PM_AddEvent(int32_t event)
{
    BG_AddPredictableEventToPlayerstate(event, 0, pm->ps);
}

void PM_AddTouchEnt(int32_t entityNum)
{
    int32_t appendIndex;
    int32_t index;

    /* All three bodies compare against 0x3fe (ENTITYNUM_WORLD).  The former
     * Linux recovered ENTITYNUM_NONE spelling was a transcription error. */
    if (entityNum == ENTITYNUM_WORLD) {
        return;
    }

    appendIndex = pm->numtouch;
    if (appendIndex == PM_MAX_TOUCH_ENTS) {
        return;
    }

    for (index = 0; index < pm->numtouch; ++index) {
        if (pm->impactEntityNums[index] == entityNum) {
            return;
        }
    }

    pm->impactEntityNums[appendIndex] = entityNum;
    pm->numtouch = coduo_int32_from_bits((uint32_t)pm->numtouch + 1u);
}

void PM_ClipVelocity(const vec3_t input, const vec3_t normal, vec3_t output,
                     float overbounce)
{
    /* Both Windows modules accumulate Y, Z, X and retain the x87 value through
     * the scale and three output expressions.  The original process control
     * word supplies PC=53; the native intermediate must retain x87 precision. */
    WIDE_FLOAT backoff =
        (WIDE_FLOAT)input[1] * normal[1] +
        (WIDE_FLOAT)normal[2] * input[2] +
        (WIDE_FLOAT)input[0] * normal[0];

    if (backoff < 0.0f) {
        backoff *= overbounce;
    } else {
        backoff /= overbounce;
    }

    output[0] = (float)(input[0] - normal[0] * backoff);
    output[1] = (float)(input[1] - normal[1] * backoff);
    output[2] = (float)(input[2] - normal[2] * backoff);
}

#if UINTPTR_MAX == UINT32_MAX
typedef char bg_pmove_trace_mask_offset[
    offsetof(pmove_t, traceMask) == 0x34 ? 1 : -1];
typedef char bg_pmove_numtouch_offset[
    offsetof(pmove_t, numtouch) == 0x54 ? 1 : -1];
typedef char bg_pmove_touch_list_offset[
    offsetof(pmove_t, impactEntityNums) == 0x58 ? 1 : -1];
typedef char bg_pmove_trace_callback_offset[
    offsetof(pmove_t, trace) == 0x104 ? 1 : -1];
#endif
