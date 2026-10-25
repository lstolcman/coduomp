#include "q_math.h"
#include "qcommon/q_shared_types.h"



static const float s_vectorCompareEpsilonSquared =
    1.0000001111620804e-6f;

/*
 * The four original Windows bodies have identical instructions except for
 * the relocated address of the shared 0x358637be threshold:
 *
 *   CoDUOMP.exe                 0x004313b0
 *   uo_cgame_mp_x86.dll        0x30049510
 *   uo_ui_mp_x86.dll           0x400014e0
 *   uo_game_mp_x86.dll         0x20016560
 *
 * They subtract first[i] - second[i], store that difference to binary32,
 * reload and square it. Unordered inputs follow the continue path. The Linux
 * bodies at coduo_lnxded
 * 0x080664a8 and game.mp.uo.i386.so RVA 0x00039ed9 instead compute the
 * subtraction twice without a spill and multiply the two x87 results. Preserve
 * the platform arithmetic variants because the binary32 spill changes the
 * squared value. FCOMP versus FUCOMPP is compiler lowering, not a second
 * source-level behavior distinction; both take the same unordered path.
 */
int32_t VectorCompareEpsilon(const vec3_t first, const vec3_t second)
{
    for (int32_t component = 0; component < 3; ++component) {
#if (defined(__i386__) || defined(__x86_64__)) && \
    (defined(__GNUC__) || defined(__clang__))
        volatile float difference;
        uint16_t status;

        __asm__ __volatile__("flds 0(%2)\n\t"
                             "fsubs 0(%3)\n\t"
                             "fstps %1\n\t"
                             "flds %1\n\t"
                             "fmuls %1\n\t"
                             "fcomps %4\n\t"
                             "fnstsw %%ax"
                             : "=a"(status), "=m"(difference)
                             : "r"(&first[component]),
                               "r"(&second[component]),
                               "m"(s_vectorCompareEpsilonSquared)
                             : "st", "memory");

        /* FCOMP sets neither C0 nor C3 only when square > epsilon. */
        if ((status & UINT16_C(0x4100)) == 0) {
            return qfalse;
        }
#else
        volatile float difference = first[component] - second[component];
        const double square =
            (double)difference * (double)difference;

        if (square > (double)s_vectorCompareEpsilonSquared) {
            return qfalse;
        }
#endif
    }

    return qtrue;
}
