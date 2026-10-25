#include "xmodel.h"


/*
 * XModel quaternion-to-axis expansion.
 *
 * Windows authority: CoDUOMP.exe 0x0049ce40..0x0049cf67.
 * Linux authority: coduo_lnxded 0x080c4eb4..0x080c5084.
 *
 * The inputs, output slots, zero-length fallback, and matrix result agree.
 * The original compilers selected different x87 operation graphs and spill
 * points, so the complete behavior bodies remain separate.  Windows retains
 * length, scale, xx/yy/zz, xy, and zw in PC=53 x87 registers.  Linux stores
 * length, scale, all scaled squares, and all pair products as binary32 under
 * the server PC=64 policy before forming the output expressions.
 */

void XModelExpandQuatToAxis(float *quat)
{
    const float q0 = quat[0];
    const float q1 = quat[1];
    const float q2 = quat[2];
    const float q3 = quat[3];

    const float q0Sq = (float)((long double)q0 * (long double)q0);
    const float q1Sq = (float)((long double)q1 * (long double)q1);
    const float q2Sq = (float)((long double)q2 * (long double)q2);
    const long double lengthSq =
        (((long double)q3 * (long double)q3 + (long double)q2Sq) +
         (long double)q1Sq) +
        (long double)q0Sq;

    if (lengthSq != (long double)0.0f) {
        const long double scale = (long double)2.0f / lengthSq;
        const long double xx = (long double)q0Sq * scale;
        const long double yy = (long double)q1Sq * scale;
        const long double zz = (long double)q2Sq * scale;
        const long double x = (long double)q0 * scale;
        const long double xy = x * (long double)q1;
        const float xz = (float)(x * (long double)q2);
        const float xw = (float)(x * (long double)q3);
        const long double y = (long double)q1 * scale;
        const float yz = (float)(y * (long double)q2);
        const float yw = (float)(y * (long double)q3);
        const long double zw =
            ((long double)q3 * (long double)q2) * scale;

        quat[0] = (float)((long double)1.0f - (zz + yy));
        quat[1] = (float)(zw + xy);
        quat[2] = (float)((long double)xz - (long double)yw);
        quat[4] = (float)(xy - zw);
        quat[5] = (float)((long double)1.0f - (zz + xx));
        quat[6] = (float)((long double)yz + (long double)xw);
        quat[8] = (float)((long double)yw + (long double)xz);
        quat[9] = (float)((long double)yz - (long double)xw);
        quat[10] = (float)((long double)1.0f - (yy + xx));
        return;
    }

    quat[0] = 1.0f;
    quat[1] = 0.0f;
    quat[2] = 0.0f;
    quat[4] = 0.0f;
    quat[5] = 1.0f;
    quat[6] = 0.0f;
    quat[8] = 0.0f;
    quat[9] = 0.0f;
    quat[10] = 1.0f;
}
