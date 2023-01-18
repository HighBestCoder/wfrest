#include <stdarg.h>
#include <stdlib.h>

#include "dc_common_assert.h"           /* dc_common_assert */
#include "dc_common_log.h"              /* dc_common_vlog   */

#include <stdlib.h>                     /* abort            */

void
dc_common_panic(const char *fmt, ...)
{
    va_list                             args;

    va_start(args, fmt);
    dc_common_vlog(DC_COMMON_LOG_ERROR, 0, fmt, args);
    va_end(args);

    abort();
}
