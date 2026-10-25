#include "q_math.h"



/*
 * The four Windows bodies are instruction-identical apart from relocated
 * constants: CoDUOMP.exe 0x00433660, cgame 0x3004b7c0, UI 0x40003790, and
 * game 0x20018810. The two Linux bodies at coduo_lnxded 0x0806977c and game
 * RVA 0x0003d3cf agree after accounting for PIC.
 *
 * Windows folds the squared magnitude as w*w + z*z + y*y + x*x. Linux folds
 * x*x + y*y + z*z + w*w and reuses the first two input lanes as the rounded
 * scaled-X/scaled-Y temporaries. The resulting formulas agree for ordinary
 * inputs, but the operation order and x87 precision differ observably. The
 * complete platform bodies preserve both original graphs and their in-place
 * read-before-write behavior.
 */
void ConvertQuatToMat(float quaternionAndMatrix[9])
{
    const float x = quaternionAndMatrix[0];
    const float y = quaternionAndMatrix[1];
    const float z = quaternionAndMatrix[2];
    const float w = quaternionAndMatrix[3];
    float xSquared;
    float ySquared;
    float zSquared;
    float lengthSquared;

    xSquared = (float)((long double)x * x);
    ySquared = (float)((long double)y * y);
    zSquared = (float)((long double)z * z);
    lengthSquared = (float)((((long double)w * w + zSquared) +
                             ySquared) + xSquared);

    if (lengthSquared != 0.0f) {
        float scale;
        float xx;
        float yy;
        float zz;
        float scaledX;
        float scaledY;
        float xy;
        float xz;
        float xw;
        float yz;
        float yw;
        float zw;

        scale = (float)((long double)2.0f / lengthSquared);
        xx = (float)((long double)xSquared * scale);
        yy = (float)((long double)ySquared * scale);
        zz = (float)((long double)zSquared * scale);
        scaledX = (float)((long double)x * scale);
        scaledY = (float)((long double)y * scale);
        xy = (float)((long double)scaledX * y);
        xz = (float)((long double)scaledX * z);
        xw = (float)((long double)scaledX * w);
        yz = (float)((long double)scaledY * z);
        yw = (float)((long double)scaledY * w);
        zw = (float)((long double)z * w * scale);

        quaternionAndMatrix[0] =
            (float)((long double)1.0f - ((long double)zz + yy));
        quaternionAndMatrix[1] = (float)((long double)zw + xy);
        quaternionAndMatrix[2] = (float)((long double)xz - yw);
        quaternionAndMatrix[3] = (float)((long double)xy - zw);
        quaternionAndMatrix[4] =
            (float)((long double)1.0f - ((long double)zz + xx));
        quaternionAndMatrix[5] = (float)((long double)yz + xw);
        quaternionAndMatrix[6] = (float)((long double)yw + xz);
        quaternionAndMatrix[7] = (float)((long double)yz - xw);
        quaternionAndMatrix[8] =
            (float)((long double)1.0f - ((long double)yy + xx));
        return;
    }

    quaternionAndMatrix[0] = 1.0f;
    quaternionAndMatrix[1] = 0.0f;
    quaternionAndMatrix[2] = 0.0f;
    quaternionAndMatrix[3] = 0.0f;
    quaternionAndMatrix[4] = 1.0f;
    quaternionAndMatrix[5] = 0.0f;
    quaternionAndMatrix[6] = 0.0f;
    quaternionAndMatrix[7] = 0.0f;
    quaternionAndMatrix[8] = 1.0f;
}
