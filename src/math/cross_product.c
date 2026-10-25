#include "q_math.h"



/*
 * The four authoritative Windows bodies are instruction-identical:
 *
 *   CoDUOMP.exe                 0x00431550
 *   uo_cgame_mp_x86.dll        0x300496b0
 *   uo_ui_mp_x86.dll           0x40001680
 *   uo_game_mp_x86.dll         0x20016700
 *
 * The two authoritative Linux bodies are likewise instruction-identical:
 * coduo_lnxded 0x080666b1 and game.mp.uo.i386.so RVA 0x0003a11f. Each lane
 * loads two pairs of binary32 operands, multiplies them without spilling,
 * subtracts the products, and stores binary32 before starting the next lane.
 * This also preserves the original lane-by-lane behavior if output aliases
 * either input.
 *
 * Windows commutes some individual multiply operands while Linux consistently
 * loads the left operand first.  The subtraction graph and every binary32
 * store are otherwise the same, so that compiler-lowering detail does not
 * justify separate source functions.  The shared x87 backend supplies the
 * selected PC=53/PC=64 arithmetic policy.
 */
void CrossProduct(const vec3_t left, const vec3_t right, vec3_t output)
{
    /* Preserve the single binary32 store per lane on native FP hosts too. */
    output[0] = (float)((long double)left[1] * (long double)right[2] -
                        (long double)left[2] * (long double)right[1]);
    output[1] = (float)((long double)left[2] * (long double)right[0] -
                        (long double)left[0] * (long double)right[2]);
    output[2] = (float)((long double)left[0] * (long double)right[1] -
                        (long double)left[1] * (long double)right[0]);
}
