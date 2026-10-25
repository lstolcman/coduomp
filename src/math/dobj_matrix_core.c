#include "q_math.h"

#include <float.h>


/*
 * Common DObj quaternion/matrix core.
 *
 * The Windows client bodies are at CoDUOMP.exe 0x004938c0, 0x00493b70,
 * 0x00493bd0, 0x00493c60, 0x004951a0, and 0x004951f0.  The corresponding
 * Linux dedicated bodies are at coduo_lnxded 0x080c7d82, 0x080c7f9a,
 * 0x080c8056, 0x080c818e, 0x080c723e, and 0x080c72ea.  Both families expose
 * the same names, signatures, layouts, aliasing behavior, and mathematical
 * transformations.  They differ in compiler-selected x87 fold and spill
 * order, so the complete platform bodies remain separate below.
 */

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
#define CODUOMP_DOBJ_NATIVE_X87 1

/* NOT_FROM_ORIGINAL_SOURCE: preserve binary32 arithmetic memory operands,
 * platform-specific float spills, and output order on native x87. Converting
 * all inputs to extended locals first changes signaling-NaN propagation.
 * All input reads finish before output writes, including overlapping arrays.
 * GNU AT&T fsubrp below encodes the Intel subtract-pop operation. */
static void coduomp_dobj_quat_multiply_native(const vec4_t lhs, const vec4_t rhs, vec4_t out)
{
    float spillZ;

    __asm__ __volatile__(
        "flds 12(%[lhs])\n\t"
        "fmuls 0(%[rhs])\n\t"
        "flds 4(%[rhs])\n\t"
        "fmuls 8(%[lhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 12(%[rhs])\n\t"
        "fmuls 0(%[lhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 8(%[rhs])\n\t"
        "fmuls 4(%[lhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 12(%[rhs])\n\t"
        "fmuls 4(%[lhs])\n\t"
        "flds 0(%[rhs])\n\t"
        "fmuls 8(%[lhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 12(%[lhs])\n\t"
        "fmuls 4(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 8(%[rhs])\n\t"
        "fmuls 0(%[lhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 12(%[rhs])\n\t"
        "fmuls 8(%[lhs])\n\t"
        "flds 4(%[lhs])\n\t"
        "fmuls 0(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 4(%[rhs])\n\t"
        "fmuls 0(%[lhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 12(%[lhs])\n\t"
        "fmuls 8(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fstps %[spillZ]\n\t"
        "flds 12(%[rhs])\n\t"
        "fmuls 12(%[lhs])\n\t"
        "flds 0(%[lhs])\n\t"
        "fmuls 0(%[rhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 4(%[lhs])\n\t"
        "fmuls 4(%[rhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 8(%[rhs])\n\t"
        "movl %[spillZ], %%eax\n\t"
        "fmuls 8(%[lhs])\n\t"
        "movl %%eax, 8(%[out])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "fstps 12(%[out])\n\t"
        "fxch %%st(1)\n\t"
        "fstps 0(%[out])\n\t"
        "fstps 4(%[out])"
        : [spillZ] "=m"(spillZ)
        : [lhs] "r"(lhs), [rhs] "r"(rhs), [out] "r"(out)
        : "eax", "memory", "st", "st(1)", "st(2)", "st(3)");
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original x87 memory operands and
 * per-platform rounding points. Matrix rows are four binary32 lanes; all
 * matrix and point reads precede the in-place output stores. */
static void coduomp_dobj_transform_in_place_native(vec3_t point, const DObjSkelMat *matrix)
{

    __asm__ __volatile__(
        "flds 32(%[matrix])\n\t"
        "fmuls 8(%[point])\n\t"
        "flds 16(%[matrix])\n\t"
        "fmuls 4(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 0(%[point])\n\t"
        "fmuls 0(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 48(%[matrix])\n\t"
        "flds 36(%[matrix])\n\t"
        "fmuls 8(%[point])\n\t"
        "flds 20(%[matrix])\n\t"
        "fmuls 4(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 4(%[matrix])\n\t"
        "fmuls 0(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 52(%[matrix])\n\t"
        "flds 40(%[matrix])\n\t"
        "fmuls 8(%[point])\n\t"
        "flds 24(%[matrix])\n\t"
        "fmuls 4(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 8(%[matrix])\n\t"
        "fmuls 0(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 56(%[matrix])\n\t"
        "fstps 8(%[point])\n\t"
        "fxch %%st(1)\n\t"
        "fstps 0(%[point])\n\t"
        "fstps 4(%[point])"
        :
        : [point] "r"(point), [matrix] "r"(matrix)
        : "memory", "st", "st(1)", "st(2)", "st(3)");
}

/* NOT_FROM_ORIGINAL_SOURCE: retain x87 arithmetic memory operands.
 * Each output store precedes the next lane's input reads; overlapping
 * point, matrix, and output storage therefore retains the original ordering. */
static void coduomp_dobj_transform_native(const vec3_t point, const DObjSkelMat *matrix, vec3_t out)
{
    __asm__ __volatile__(
        "flds 32(%[matrix])\n\t"
        "fmuls 8(%[point])\n\t"
        "flds 16(%[matrix])\n\t"
        "fmuls 4(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 0(%[point])\n\t"
        "fmuls 0(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 48(%[matrix])\n\t"
        "fstps 0(%[out])\n\t"
        "flds 4(%[matrix])\n\t"
        "fmuls 0(%[point])\n\t"
        "flds 36(%[matrix])\n\t"
        "fmuls 8(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 20(%[matrix])\n\t"
        "fmuls 4(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 52(%[matrix])\n\t"
        "fstps 4(%[out])\n\t"
        "flds 8(%[matrix])\n\t"
        "fmuls 0(%[point])\n\t"
        "flds 40(%[matrix])\n\t"
        "fmuls 8(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 24(%[matrix])\n\t"
        "fmuls 4(%[point])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 56(%[matrix])\n\t"
        "fstps 8(%[out])"
        :
        : [point] "r"(point), [matrix] "r"(matrix), [out] "r"(out)
        : "memory", "st", "st(1)");
}

/* NOT_FROM_ORIGINAL_SOURCE: retain x87 arithmetic memory operands.
 * Translation is retained in x87 on Windows and spilled to binary32 on
 * Linux. Later matrix rows are read after earlier output stores. */
static void coduomp_dobj_inverse_transform_native(const vec3_t point, const DObjSkelMat *matrix, vec3_t out)
{
    __asm__ __volatile__(
        "flds 0(%[point])\n\t"
        "fsubs 48(%[matrix])\n\t"
        "flds 4(%[point])\n\t"
        "fsubs 52(%[matrix])\n\t"
        "flds 8(%[point])\n\t"
        "fsubs 56(%[matrix])\n\t"
        "fld %%st(0)\n\t"
        "fmuls 8(%[matrix])\n\t"
        "fld %%st(2)\n\t"
        "fmuls 4(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fld %%st(3)\n\t"
        "fmuls 0(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fstps 0(%[out])\n\t"
        "fld %%st(0)\n\t"
        "fmuls 24(%[matrix])\n\t"
        "fld %%st(2)\n\t"
        "fmuls 20(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fld %%st(3)\n\t"
        "fmuls 16(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fstps 4(%[out])\n\t"
        "fmuls 40(%[matrix])\n\t"
        "fxch %%st(1)\n\t"
        "fmuls 36(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fxch %%st(1)\n\t"
        "fmuls 32(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fstps 8(%[out])"
        :
        : [point] "r"(point), [matrix] "r"(matrix), [out] "r"(out)
        : "memory", "st", "st(1)", "st(2)", "st(3)", "st(4)");
}

#else
#define CODUOMP_DOBJ_NATIVE_X87 0
#endif


/* NOT_FROM_ORIGINAL_SOURCE: round an extended matrix result directly into its
 * output lane, without reloading a binary32 temporary before publication. */
static void coduomp_dobj_store_extended_lane(float *lane, long double value)
{
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) && LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384
    __asm__ __volatile__(
        "fldt %[value]\n\t"
        "fstps %[lane]"
        : [lane] "=m" (*lane)
        : [value] "m" (value)
        : "memory", "st");
#else
    *lane = value;
#endif
}

void DObjQuatToMatrix43(const vec4_t quat, DObjSkelMat *matrix)
{
    float q0Sq = quat[0] * quat[0];
    float q1Sq = quat[1] * quat[1];
    float q2Sq = quat[2] * quat[2];
    /* Keep the three squared-vector spills, while retaining the norm and
     * scale until the individual matrix-lane stores. */
    long double lengthSq =
        (((long double)quat[3] * quat[3] + q2Sq) + q1Sq) + q0Sq;

    if (lengthSq != 0.0f) {
        long double scale = 2.0L / lengthSq;
        long double xx = q0Sq * scale;
        long double yy = q1Sq * scale;
        long double zz = q2Sq * scale;
        long double xScale = quat[0] * scale;
        long double xy = xScale * quat[1];
        float xz = (float)(xScale * quat[2]);
        float xw = (float)(xScale * quat[3]);
        long double yScale = quat[1] * scale;
        float yz = (float)(yScale * quat[2]);
        float yw = (float)(yScale * quat[3]);
        long double zw = (long double)quat[3] * quat[2];

        /* Publish the padding before the final scale operation and round each
         * extended result directly into its output lane without a float reload. */
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][3] = 0.0f;
        zw *= scale;

        coduomp_dobj_store_extended_lane(&matrix->axis[0][0], 1.0L - (yy + zz));
        coduomp_dobj_store_extended_lane(&matrix->axis[0][1], xy + zw);
        matrix->axis[0][2] = xz - yw;
        coduomp_dobj_store_extended_lane(&matrix->axis[1][0], xy - zw);
        coduomp_dobj_store_extended_lane(&matrix->axis[1][1], 1.0L - (xx + zz));
        matrix->axis[1][2] = yz + xw;
        matrix->axis[2][0] = xz + yw;
        matrix->axis[2][1] = yz - xw;
        coduomp_dobj_store_extended_lane(&matrix->axis[2][2], 1.0L - (xx + yy));
    } else {
        matrix->axis[0][0] = 1.0f;
        matrix->axis[0][1] = 0.0f;
        matrix->axis[0][2] = 0.0f;
        matrix->axis[1][0] = 0.0f;
        matrix->axis[1][1] = 1.0f;
        matrix->axis[1][2] = 0.0f;
        matrix->axis[2][0] = 0.0f;
        matrix->axis[2][1] = 0.0f;
        matrix->axis[2][2] = 1.0f;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][3] = 0.0f;
    }
}

void DObjMatrixTransformVector43InPlace(
    vec3_t point, const DObjSkelMat *matrix)
{
#if CODUOMP_DOBJ_NATIVE_X87
    coduomp_dobj_transform_in_place_native(point, matrix);
#else
    float x = point[0];
    float y = point[1];
    float z = point[2];

    /* Preserve all input lanes until the in-place output stores. */
    long double outX =
        (((long double)z * matrix->axis[2][0] + (long double)y * matrix->axis[1][0]) +
         (long double)x * matrix->axis[0][0]) +
        matrix->origin[0];
    long double outY =
        (((long double)z * matrix->axis[2][1] + (long double)y * matrix->axis[1][1]) +
         (long double)x * matrix->axis[0][1]) +
        matrix->origin[1];
    long double outZ =
        (((long double)z * matrix->axis[2][2] + (long double)y * matrix->axis[1][2]) +
         (long double)x * matrix->axis[0][2]) +
        matrix->origin[2];

    point[2] = (float)outZ;
    point[0] = (float)outX;
    point[1] = (float)outY;
#endif
}

void DObjMatrixTransformVector43(const vec3_t point,
                                 const DObjSkelMat *matrix,
                                 vec3_t out)
{
#if CODUOMP_DOBJ_NATIVE_X87
    coduomp_dobj_transform_native(point, matrix, out);
#else
    out[0] = (float)((((long double)point[2] * matrix->axis[2][0] +
                      (long double)point[1] * matrix->axis[1][0]) +
                     (long double)point[0] * matrix->axis[0][0]) + matrix->origin[0]);
    out[1] = (float)((((long double)point[0] * matrix->axis[0][1] +
                      (long double)point[2] * matrix->axis[2][1]) +
                     (long double)point[1] * matrix->axis[1][1]) + matrix->origin[1]);
    out[2] = (float)((((long double)point[0] * matrix->axis[0][2] +
                      (long double)point[2] * matrix->axis[2][2]) +
                     (long double)point[1] * matrix->axis[1][2]) + matrix->origin[2]);
#endif
}

void DObjMatrixInverseTransformVector43(
    const vec3_t point, const DObjSkelMat *matrix, vec3_t out)
{
#if CODUOMP_DOBJ_NATIVE_X87
    coduomp_dobj_inverse_transform_native(point, matrix, out);
#else
    const long double x =
        (long double)point[0] - (long double)matrix->origin[0];
    const long double y =
        (long double)point[1] - (long double)matrix->origin[1];
    const long double z =
        (long double)point[2] - (long double)matrix->origin[2];

    out[0] = (float)((z * (long double)matrix->axis[0][2] +
                       y * (long double)matrix->axis[0][1]) +
                      x * (long double)matrix->axis[0][0]);
    out[1] = (float)((z * (long double)matrix->axis[1][2] +
                       y * (long double)matrix->axis[1][1]) +
                      x * (long double)matrix->axis[1][0]);
    out[2] = (float)((z * (long double)matrix->axis[2][2] +
                       y * (long double)matrix->axis[2][1]) +
                      x * (long double)matrix->axis[2][0]);
#endif
}

void DObjQuatMultiplyIntoFirst(vec4_t quat, const vec4_t rhs)
{
#if CODUOMP_DOBJ_NATIVE_X87
    coduomp_dobj_quat_multiply_native(quat, rhs, quat);
#else
    long double rhsW = rhs[3];
    long double quatW = quat[3];
    long double rhsX = rhs[0];
    long double rhsY = rhs[1];
    long double rhsZ = rhs[2];
    long double quatX = quat[0];
    long double quatY = quat[1];
    long double quatZ = quat[2];

    long double outX = ((quatW * rhsX + rhsY * quatZ) + rhsW * quatX) - quatY * rhsZ;
    long double outY = ((quatY * rhsW - quatZ * rhsX) + quatW * rhsY) + quatX * rhsZ;
    /* Z is spilled separately; the other quaternion lanes retain their intermediates. */
    float outZ = (float)(((quatZ * rhsW + quatY * rhsX) - quatX * rhsY) + quatW * rhsZ);
    long double outW = ((quatW * rhsW - quatX * rhsX) - quatY * rhsY) - quatZ * rhsZ;
    quat[2] = outZ;
    quat[3] = (float)outW;
    quat[0] = (float)outX;
    quat[1] = (float)outY;
#endif
}

void DObjQuatMultiplyIntoSecond(const vec4_t lhs, vec4_t quat)
{
#if CODUOMP_DOBJ_NATIVE_X87
    coduomp_dobj_quat_multiply_native(lhs, quat, quat);
#else
    long double lhsX = lhs[0];
    long double rhsW = quat[3];
    long double lhsW = lhs[3];
    long double lhsZ = lhs[2];
    long double lhsY = lhs[1];
    long double rhsX = quat[0];
    long double rhsY = quat[1];
    long double rhsZ = quat[2];

    long double outX = ((lhsW * rhsX + rhsY * lhsZ) + rhsW * lhsX) - lhsY * rhsZ;
    long double outY = ((lhsY * rhsW - lhsZ * rhsX) + lhsW * rhsY) + lhsX * rhsZ;
    float outZ = (float)(((lhsZ * rhsW + lhsY * rhsX) - lhsX * rhsY) + lhsW * rhsZ);

    long double outW = ((lhsW * rhsW - lhsX * rhsX) - lhsY * rhsY) - lhsZ * rhsZ;
    quat[2] = outZ;
    quat[3] = (float)outW;
    quat[0] = (float)outX;
    quat[1] = (float)outY;
#endif
}
