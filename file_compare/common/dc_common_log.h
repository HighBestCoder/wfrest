#ifndef _DC_COMMON_LOG_H_
#define _DC_COMMON_LOG_H_

#include "dc_common_cdefs.h"            /* __BEGIN_DECLS    */

#include <stdarg.h>                     /* va_list          */

/*
 * DO NOT CHANGE THESE.
 * DO NOT ADD MORE VALUES.
 */
#define DC_COMMON_LOG_ERROR	            0
#define DC_COMMON_LOG_WARN	            1
#define DC_COMMON_LOG_INFO	            2

typedef void                            (*dc_common_vloglevel_fn_t)(int, int, const char *, va_list);


#ifndef DC_COMMON_VLOG_FN_T_DEFINED     /* 如果没有DC_COMMON_VLOG_FN_T_DEFINED         */
    #define DC_COMMON_VLOG_FN_T_DEFINED
    typedef void                        (*dc_common_vlog_fn_t)(int, const char *, va_list);
#endif                                  /* DC_COMMON_VLOG_FN_T_DEFINED               */


__BEGIN_DECLS                           // C++ Wrap Macro

void
dc_common_log(int facility,
              int level,
              const char *format,
              ...) __printflike(3, 4);

void
dc_common_vlog(int facility,
               int level,
               const char *format,
               va_list args);

void
dc_common_vlog_register(dc_common_vlog_fn_t vlog_func);

void
dc_common_log_setlevel(int level);

__END_DECLS                             // End C++ Wrap Macro

#endif	/* !_DC_COMMON_LOG_H_ */
