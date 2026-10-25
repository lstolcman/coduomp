#include "q_math.h"

#include "compat/coduo_native_x87.h"

#include <limits.h>
#include <math.h>

/* Source: CoDUOMP.exe 0x00408e90..0x00408eb0.
 * Evidence: the exact qword at 0x005b9d00 is 2^-30. The x87 body adds that
 * binary64 bias to the binary32 input and converts with FISTP under the
 * process round-to-nearest policy. Name: exact Mac client symbol FastRound. */
int32_t FastRound(float value)
{
    const double rounded = rint((double)value + 0x1p-30);

    if (!(rounded >= (double)INT32_MIN && rounded <= (double)INT32_MAX))
        return INT32_MIN;

    return (int32_t)rounded;
}
