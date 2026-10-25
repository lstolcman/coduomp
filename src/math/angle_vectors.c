#include "q_math.h"

#include "compat/coduo_native_x87.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>


/*
 * YawVectors immediately follows AngleVectors in all six images.  The four
 * Windows bodies are byte-identical apart from the relocated pi/180 constant:
 *
 *   CoDUOMP.exe                 0x00432200
 *   uo_cgame_mp_x86.dll        0x3004a360
 *   uo_ui_mp_x86.dll           0x40002330
 *   uo_game_mp_x86.dll         0x200173b0
 *
 * The cgame/UI reconstruction's YawToPerpVectors name and output-first source
 * signature were artifacts of recovering the compiler's register allocation;
 * the Linux YawVectors symbol and the engine/game source identity establish
 * the canonical source contract.  Linux coduo_lnxded 0x0806763f and
 * game.mp.uo.i386.so RVA 0x0003b201 use a binary64 pi/180 constant and toggle
 * the stored cosine sign bit with integer XOR.  Windows uses the binary32
 * constant and x87 FCHS.  The complete platform bodies retain those genuine
 * source-level differences.
 */
void YawVectors(float yaw, vec3_t forward, vec3_t right)
{
    float radians;
    float sine;
    float cosine;

    radians = (float)((long double)yaw * 0.017453292f);
    coduo_x87_sincosf(radians, &sine, &cosine);

    if (forward != NULL) {
        forward[0] = cosine;
        forward[1] = sine;
        forward[2] = 0.0f;
    }
    if (right != NULL) {
        right[0] = sine;
        right[1] = -cosine;
        right[2] = 0.0f;
    }
}

/*
 * The YawToAxis wrapper is shared by all six images.  Its four Windows bodies
 * are instruction-identical apart from relocated calls and vec3_origin:
 * CoDUOMP.exe 0x004340f0, cgame 0x3004c250, UI 0x40004260, and game
 * 0x200192a0.  The Linux bodies at coduo_lnxded 0x0806a4cc and game module
 * RVA 0x0003e2c0 agree after accounting for PIC.  Linux emits the constant up
 * row before the three subtractions while the optimized Windows compiler
 * schedules those independent stores between them; the computed matrix and
 * all dependencies are identical.  Subtracting from vec3_origin, rather than
 * applying unary minus, retains the original signed-zero behavior.
 */
void YawToAxis(float yaw, axis_t axis)
{
    vec3_t right;

    YawVectors(yaw, axis[0], right);
    axis[2][0] = 0.0f;
    axis[2][1] = 0.0f;
    axis[2][2] = 1.0f;
    axis[1][0] = vec3_origin[0] - right[0];
    axis[1][1] = vec3_origin[1] - right[1];
    axis[1][2] = vec3_origin[2] - right[2];
}


/*
 * AngleVectors is shared by every authoritative image.  The four Windows
 * bodies are instruction-identical apart from relocated constants:
 *
 *   CoDUOMP.exe                 0x004320a0
 *   uo_cgame_mp_x86.dll        0x3004a200
 *   uo_ui_mp_x86.dll           0x400021d0
 *   uo_game_mp_x86.dll         0x20017250
 *
 * They multiply angles by a binary32 pi/180 constant and spill sr*sp and cr*sp
 * before reusing those products.  The two Linux bodies at coduo_lnxded
 * 0x080674ad and game.mp.uo.i386.so RVA 0x0003b068 instead load a binary64
 * pi/180 constant and retain the longer right/up multiply chains.  Their
 * instruction graphs agree after accounting for the game module's PIC base.
 * The maintained Windows body preserves its operation graph and explicit
 * stores using native arithmetic on every host.
 */

void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right,
                  vec3_t up)
{
    float sy;
    float cy;
    float sp;
    float cp;
    float angle;

    angle = (float)((long double)angles[1] * 0.017453292f);
    coduo_x87_sincosf(angle, &sy, &cy);

    angle = (float)((long double)angles[0] * 0.017453292f);
    coduo_x87_sincosf(angle, &sp, &cp);

    if (forward != NULL) {
        forward[0] = (float)((long double)cp * cy);
        forward[1] = (float)((long double)cp * sy);
        forward[2] = -sp;
    }

    if (right != NULL || up != NULL) {
        float sr;
        float cr;

        angle = (float)((long double)angles[2] * 0.017453292f);
        coduo_x87_sincosf(angle, &sr, &cr);

        if (right != NULL) {
            const float srSp = (float)((long double)sr * sp);

            right[0] = (float)((long double)cr * sy -
                               (long double)srSp * cy);
            right[1] = (float)((long double)cr * cy * -1.0f -
                               (long double)srSp * sy);
            right[2] = (float)((long double)sr * cp * -1.0f);
        }

        if (up != NULL) {
            const float crSp = (float)((long double)cr * sp);

            up[0] = (float)((long double)crSp * cy +
                            (long double)sr * sy);
            up[1] = (float)((long double)crSp * sy -
                            (long double)sr * cy);
            up[2] = (float)((long double)cr * cp);
        }
    }
}
