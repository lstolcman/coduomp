#include "coduo_libm.h"
#include "compat/crt/sqrt_compat.h"

/* NOT_FROM_ORIGINAL_SOURCE: binary64 adapter for callers that explicitly own
 * both double boundaries. Live x87 callers use coduomp_crt_sqrtl directly. */
double CoduoLibm_Sqrt(double value)
{
    return (double)coduomp_crt_sqrtl(value);
}
