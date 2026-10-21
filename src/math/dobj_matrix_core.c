#include "q_math.h"

#if defined(WINDOWS_BEHAVIOR) && defined(LINUX_BEHAVIOR)
#error "Select only one of WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#elif !defined(WINDOWS_BEHAVIOR) && !defined(LINUX_BEHAVIOR)
#error "Select WINDOWS_BEHAVIOR or LINUX_BEHAVIOR"
#endif

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
#if defined(WINDOWS_BEHAVIOR)
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
#else
    float spillX, spillY, spillZ;

    __asm__ __volatile__(
        "flds 0(%[lhs])\n\t"
        "fmuls 12(%[rhs])\n\t"
        "flds 12(%[lhs])\n\t"
        "fmuls 0(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 8(%[lhs])\n\t"
        "fmuls 4(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 4(%[lhs])\n\t"
        "fmuls 8(%[rhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "fstps %[spillX]\n\t"
        "flds 4(%[lhs])\n\t"
        "fmuls 12(%[rhs])\n\t"
        "flds 8(%[lhs])\n\t"
        "fmuls 0(%[rhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 12(%[lhs])\n\t"
        "fmuls 4(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 0(%[lhs])\n\t"
        "fmuls 8(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fstps %[spillY]\n\t"
        "flds 8(%[lhs])\n\t"
        "fmuls 12(%[rhs])\n\t"
        "flds 4(%[lhs])\n\t"
        "fmuls 0(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 0(%[lhs])\n\t"
        "fmuls 4(%[rhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 12(%[lhs])\n\t"
        "fmuls 8(%[rhs])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fstps %[spillZ]\n\t"
        "flds 12(%[lhs])\n\t"
        "fmuls 12(%[rhs])\n\t"
        "flds 0(%[lhs])\n\t"
        "fmuls 0(%[rhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 4(%[lhs])\n\t"
        "fmuls 4(%[rhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "flds 8(%[lhs])\n\t"
        "fmuls 8(%[rhs])\n\t"
        "fsubrp %%st, %%st(1)\n\t"
        "fstps 12(%[out])\n\t"
        "movl %[spillX], %%eax\n\t"
        "movl %%eax, 0(%[out])\n\t"
        "movl %[spillY], %%eax\n\t"
        "movl %%eax, 4(%[out])\n\t"
        "movl %[spillZ], %%eax\n\t"
        "movl %%eax, 8(%[out])"
        : [spillX] "=m"(spillX), [spillY] "=m"(spillY), [spillZ] "=m"(spillZ)
        : [lhs] "r"(lhs), [rhs] "r"(rhs), [out] "r"(out)
        : "eax", "memory", "st", "st(1)", "st(2)", "st(3)");
#endif
}

/* NOT_FROM_ORIGINAL_SOURCE: retain the original x87 memory operands and
 * per-platform rounding points. Matrix rows are four binary32 lanes; all
 * matrix and point reads precede the in-place output stores. */
static void coduomp_dobj_transform_in_place_native(vec3_t point, const DObjSkelMat *matrix)
{
#if defined(WINDOWS_BEHAVIOR)

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
#else
    float spillX, spillY;

    __asm__ __volatile__(
        "flds 0(%[point])\n\t"
        "fmuls 0(%[matrix])\n\t"
        "flds 4(%[point])\n\t"
        "fmuls 16(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 8(%[point])\n\t"
        "fmuls 32(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 48(%[matrix])\n\t"
        "fstps %[spillX]\n\t"
        "flds 0(%[point])\n\t"
        "fmuls 4(%[matrix])\n\t"
        "flds 4(%[point])\n\t"
        "fmuls 20(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 8(%[point])\n\t"
        "fmuls 36(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 52(%[matrix])\n\t"
        "fstps %[spillY]\n\t"
        "flds 0(%[point])\n\t"
        "fmuls 8(%[matrix])\n\t"
        "flds 4(%[point])\n\t"
        "fmuls 24(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "flds 8(%[point])\n\t"
        "fmuls 40(%[matrix])\n\t"
        "faddp %%st, %%st(1)\n\t"
        "fadds 56(%[matrix])\n\t"
        "fstps 8(%[point])\n\t"
        "movl %[spillX], %%eax\n\t"
        "movl %%eax, 0(%[point])\n\t"
        "movl %[spillY], %%eax\n\t"
        "movl %%eax, 4(%[point])"
        : [spillX] "=m"(spillX), [spillY] "=m"(spillY)
        : [point] "r"(point), [matrix] "r"(matrix)
        : "eax", "memory", "st", "st(1)", "st(2)", "st(3)");
#endif
}

#else
#define CODUOMP_DOBJ_NATIVE_X87 0
#endif

#if defined(WINDOWS_BEHAVIOR)

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
        long double zw = ((long double)quat[2] * quat[3]) * scale;

        matrix->axis[0][0] = (float)(1.0L - (yy + zz));
        matrix->axis[0][1] = (float)(xy + zw);
        matrix->axis[0][2] = xz - yw;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][0] = (float)(xy - zw);
        matrix->axis[1][1] = (float)(1.0L - (xx + zz));
        matrix->axis[1][2] = yz + xw;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][0] = xz + yw;
        matrix->axis[2][1] = yz - xw;
        matrix->axis[2][2] = (float)(1.0L - (xx + yy));
        matrix->axis[2][3] = 0.0f;
    } else {
        matrix->axis[0][0] = 1.0f;
        matrix->axis[0][1] = 0.0f;
        matrix->axis[0][2] = 0.0f;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][0] = 0.0f;
        matrix->axis[1][1] = 1.0f;
        matrix->axis[1][2] = 0.0f;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][0] = 0.0f;
        matrix->axis[2][1] = 0.0f;
        matrix->axis[2][2] = 1.0f;
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
    out[0] = (float)((((long double)point[2] * matrix->axis[2][0] +
                      (long double)point[1] * matrix->axis[1][0]) +
                     (long double)point[0] * matrix->axis[0][0]) + matrix->origin[0]);
    out[1] = (float)((((long double)point[0] * matrix->axis[0][1] +
                      (long double)point[2] * matrix->axis[2][1]) +
                     (long double)point[1] * matrix->axis[1][1]) + matrix->origin[1]);
    out[2] = (float)((((long double)point[0] * matrix->axis[0][2] +
                      (long double)point[2] * matrix->axis[2][2]) +
                     (long double)point[1] * matrix->axis[1][2]) + matrix->origin[2]);
}

void DObjMatrixInverseTransformVector43(
    const vec3_t point, const DObjSkelMat *matrix, vec3_t out)
{
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

#else

void DObjMatrixTransformVector43(const vec3_t in,
                                 const DObjSkelMat *matrix,
                                 vec3_t out)
{
    out[0] = in[0] * matrix->axis[0][0] +
             in[1] * matrix->axis[1][0] +
             in[2] * matrix->axis[2][0] + matrix->origin[0];
    out[1] = in[0] * matrix->axis[0][1] +
             in[1] * matrix->axis[1][1] +
             in[2] * matrix->axis[2][1] + matrix->origin[1];
    out[2] = in[0] * matrix->axis[0][2] +
             in[1] * matrix->axis[1][2] +
             in[2] * matrix->axis[2][2] + matrix->origin[2];
}

void DObjMatrixInverseTransformVector43(const vec3_t in,
                                        const DObjSkelMat *matrix,
                                        vec3_t out)
{
    float translated0 = in[0] - matrix->origin[0];
    float translated1 = in[1] - matrix->origin[1];
    float translated2 = in[2] - matrix->origin[2];

    out[0] = translated0 * matrix->axis[0][0] +
             translated1 * matrix->axis[0][1] +
             translated2 * matrix->axis[0][2];
    out[1] = translated0 * matrix->axis[1][0] +
             translated1 * matrix->axis[1][1] +
             translated2 * matrix->axis[1][2];
    out[2] = translated0 * matrix->axis[2][0] +
             translated1 * matrix->axis[2][1] +
             translated2 * matrix->axis[2][2];
}

void DObjQuatToMatrix43(const vec4_t quat, DObjSkelMat *matrix)
{
    float q0Sq = quat[0] * quat[0];
    float q1Sq = quat[1] * quat[1];
    float q2Sq = quat[2] * quat[2];
    float lengthSq = ((q0Sq + q1Sq) + q2Sq) + quat[3] * quat[3];

    if (lengthSq != 0.0f) {
        float scale = 2.0f / lengthSq;
        float xx = q0Sq * scale;
        float yy = q1Sq * scale;
        float zz = q2Sq * scale;
        float xScale = quat[0] * scale;
        float xy = xScale * quat[1];
        float xz = xScale * quat[2];
        float xw = xScale * quat[3];
        float yScale = quat[1] * scale;
        float yz = yScale * quat[2];
        float yw = yScale * quat[3];
        float zw = (quat[2] * quat[3]) * scale;

        matrix->axis[0][0] = 1.0f - (yy + zz);
        matrix->axis[0][1] = xy + zw;
        matrix->axis[0][2] = xz - yw;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][0] = xy - zw;
        matrix->axis[1][1] = 1.0f - (xx + zz);
        matrix->axis[1][2] = yz + xw;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][0] = xz + yw;
        matrix->axis[2][1] = yz - xw;
        matrix->axis[2][2] = 1.0f - (xx + yy);
        matrix->axis[2][3] = 0.0f;
    } else {
        matrix->axis[0][0] = 1.0f;
        matrix->axis[0][1] = 0.0f;
        matrix->axis[0][2] = 0.0f;
        matrix->axis[0][3] = 0.0f;
        matrix->axis[1][0] = 0.0f;
        matrix->axis[1][1] = 1.0f;
        matrix->axis[1][2] = 0.0f;
        matrix->axis[1][3] = 0.0f;
        matrix->axis[2][0] = 0.0f;
        matrix->axis[2][1] = 0.0f;
        matrix->axis[2][2] = 1.0f;
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
    float outX =
        x * matrix->axis[0][0] + y * matrix->axis[1][0] +
        z * matrix->axis[2][0] + matrix->origin[0];
    float outY =
        x * matrix->axis[0][1] + y * matrix->axis[1][1] +
        z * matrix->axis[2][1] + matrix->origin[1];

    point[2] = x * matrix->axis[0][2] + y * matrix->axis[1][2] +
               z * matrix->axis[2][2] + matrix->origin[2];
    point[0] = outX;
    point[1] = outY;
#endif
}

void DObjQuatMultiplyIntoFirst(vec4_t quat, const vec4_t rhs)
{
#if CODUOMP_DOBJ_NATIVE_X87
    coduomp_dobj_quat_multiply_native(quat, rhs, quat);
#else
    float rhsW = rhs[3];
    float quatW = quat[3];
    float rhsX = rhs[0];
    float rhsY = rhs[1];
    float rhsZ = rhs[2];
    float quatX = quat[0];
    float quatY = quat[1];
    float quatZ = quat[2];

    float outX = ((quatX * rhsW + quatW * rhsX) + quatZ * rhsY) -
                 quatY * rhsZ;
    float outY = ((quatY * rhsW - quatZ * rhsX) + quatW * rhsY) +
                 quatX * rhsZ;
    float outZ = ((quatZ * rhsW + quatY * rhsX) - quatX * rhsY) +
                 quatW * rhsZ;

    quat[3] = ((quatW * rhsW - quatX * rhsX) - quatY * rhsY) -
              quatZ * rhsZ;
    quat[0] = outX;
    quat[1] = outY;
    quat[2] = outZ;
#endif
}

void DObjQuatMultiplyIntoSecond(const vec4_t lhs, vec4_t quat)
{
#if CODUOMP_DOBJ_NATIVE_X87
    coduomp_dobj_quat_multiply_native(lhs, quat, quat);
#else
    float lhsX = lhs[0];
    float rhsW = quat[3];
    float lhsW = lhs[3];
    float lhsZ = lhs[2];
    float lhsY = lhs[1];
    float rhsX = quat[0];
    float rhsY = quat[1];
    float rhsZ = quat[2];

    float outX = ((lhsX * rhsW + lhsW * rhsX) + lhsZ * rhsY) - lhsY * rhsZ;
    float outY = ((lhsY * rhsW - lhsZ * rhsX) + lhsW * rhsY) + lhsX * rhsZ;
    float outZ = ((lhsZ * rhsW + lhsY * rhsX) - lhsX * rhsY) + lhsW * rhsZ;

    quat[3] = ((lhsW * rhsW - lhsX * rhsX) - lhsY * rhsY) - lhsZ * rhsZ;
    quat[0] = outX;
    quat[1] = outY;
    quat[2] = outZ;
#endif
}

#endif
