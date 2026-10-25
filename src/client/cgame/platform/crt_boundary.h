#ifndef CLIENT_CRT_BOUNDARY_H
#define CLIENT_CRT_BOUNDARY_H

#include "client/common/client_legacy_crt.h"
#include "compat/coduo_fp_conversion.h"
#include "compat/crt/format_compat.h"
#include "compat/crt/random_compat.h"

/*
 * NOT_FROM_ORIGINAL_SOURCE
 *
 * The Windows uo_cgame_mp_x86.dll contains statically linked MSVC CRT/runtime
 * code. Those machine-code functions are part of the binary inventory, but
 * they are not CoD client game source and must not be reconstructed under
 * src/client/cgame/functions/.
 *
 * The original Windows CRT's observable behavior is part of the recovered
 * client contract. Reimplement it in shared compatibility code where host
 * facilities differ; static-runtime classification does not exempt a service
 * from that requirement. The includes above are one part of that boundary.
 * Host facilities are suitable only where they preserve the supported caller
 * contract. Native floating-point limits do not waive other CRT semantics.
 */

#endif /* CLIENT_CRT_BOUNDARY_H */
