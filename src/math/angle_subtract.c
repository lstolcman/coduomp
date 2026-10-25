#include "q_math.h"



/*
 * All six authoritative LerpAngle bodies retain the same working binary32
 * target, two single-wrap tests, and final multiply/add graph:
 *
 *   CoDUOMP.exe                 0x00433ba0
 *   uo_cgame_mp_x86.dll        0x3004bd00
 *   uo_ui_mp_x86.dll           0x40003cd0
 *   uo_game_mp_x86.dll         0x20018d50
 *   coduo_lnxded               0x08069deb
 *   game.mp.uo.i386.so         RVA 0x0003db20
 *
 * The four Windows bodies are instruction-identical apart from constant
 * addresses.  The Linux bodies use FUCOMPP instead of the Windows FCOMP/status
 * tests and run under PC=64 rather than PC=53, but preserve the arithmetic,
 * branch results, and store order.  The comparison opcode is a lowering
 * detail; the maintained Windows graph uses native wide intermediates.
 * The result is explicitly narrowed to binary32 before return in every body.
 */
float LerpAngle(float from, float to, float fraction)
{
    float target = to;

    long double difference = (long double)to - (long double)from;

    if (difference > 180.0L) {
        target = (float)((long double)to - 360.0L);
    }

    difference = (long double)target - (long double)from;
    if (difference < -180.0L) {
        target = (float)((long double)target + 360.0L);
    }

    difference = (long double)target - (long double)from;
    /* Preserve the original separate FMUL then FADD operation graph.  On an
     * x87 target this carrier remains subject to the ambient PC=53/PC=64
     * control word; it does not impose a platform-specific C type. */
    volatile long double product = difference * (long double)fraction;
    return (float)(product + (long double)from);
}

/*
 * All six authoritative AngleSubtract bodies perform the same operation graph:
 *
 *   CoDUOMP.exe                 0x00433c10
 *   uo_cgame_mp_x86.dll        0x3004bd70
 *   uo_ui_mp_x86.dll           0x40003d40
 *   uo_game_mp_x86.dll         0x20018dc0
 *   coduo_lnxded               0x08069e5b
 *   game.mp.uo.i386.so         RVA 0x0003dba0
 *
 * The initial difference and every +/-360-degree update are stored to
 * binary32 before the next comparison.  Windows uses x87 PC=53 and Linux uses
 * x87 PC=64, but subtraction of two binary32 values (or a binary32 value and
 * exact 360.0) is exact at both precisions before that common binary32 store.
 * The Windows bodies are instruction-identical apart from image-local constant
 * addresses; the two Linux bodies retain the same dataflow with PIC framing.
 */
float AngleSubtract(float first, float second)
{
    /* Volatile is intentional: the original bodies use this binary32 memory
     * slot as their accumulator, so an i386 compiler must not retain an
     * excess-precision x87 value between the store and the next comparison. */
    volatile float delta = first - second;

    /* NOT_FROM_ORIGINAL_SOURCE: preserve this recovered boundary's validated input, state, and compatibility invariants. */
    while (delta > 180.0f) {
        const float previous = delta;

        delta -= 360.0f;
        if (delta == previous) {
            break;
        }
    }
    while (delta < -180.0f) {
        const float previous = delta;

        delta += 360.0f;
        if (delta == previous) {
            break;
        }
    }

    return delta;
}

/*
 * The corresponding bodies at 0x00433c90, 0x3004bdf0, 0x40003dc0,
 * 0x20018e40, 0x08069ebc, and Linux-game RVA 0x0003dc11 call AngleSubtract
 * for lanes 0, 1, and 2 in ascending order and store each binary32 result
 * immediately.  That order permits result to alias either input exactly as in
 * the original bodies.
 */
void AnglesSubtract(const vec3_t first, const vec3_t second, vec3_t result)
{
    result[0] = AngleSubtract(first[0], second[0]);
    result[1] = AngleSubtract(first[1], second[1]);
    result[2] = AngleSubtract(first[2], second[2]);
}
