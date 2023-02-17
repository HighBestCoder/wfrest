#ifndef _DC_LOG_TRACE_LOG_H_
#define _DC_LOG_TRACE_LOG_H_

#include "dc_common_assert.h"           /* DC_COMMON_ASSERT */
#include "dc_common_log.h"              /* dc_common_log    */
#include "dc_common_error.h"            /* E_ARG_INVALID    */

#include <stdio.h>                      /* stderr/fprintf   */
#include <limits.h>                     /* PATH_MAX         */

static void
dc_common_trace_log1(const char* file_name,
               const char* func_name,
               const int line_number,
               const int facility,
               const char* format,
               va_list args)
{
    char                                str[1024];

    vsnprintf(str, sizeof(str), format, args);

    dc_common_log(facility,
            0 /*level*/,
            "%s at %s(%s:%d)",
            str,
            func_name,
            file_name,
            line_number);
}

static void __dc_unused
dc_common_trace_log(const char* file_name,
              const char* func_name,
              const int line_number,
              const int facility,
              const char* format,
              ...)
{
    va_list                             args;

    va_start(args, format);

    dc_common_trace_log1(file_name,
                   func_name,
                   line_number,
                   facility,
                   format,
                   args);

    va_end(args);
}

#define LOG(...) dc_common_trace_log(__FILE__,              \
                         __func__,                          \
                         __LINE__,                          \
                         __VA_ARGS__)

#define LOG_ROOT_ERR(ret, ...)    do {                      \
    LOG(DC_COMMON_LOG_ERROR, __VA_ARGS__);                  \
    LOG(DC_COMMON_LOG_ERROR,                                \
        "StackTrace: %s\n\t\t\t\t\t\t",                     \
        #ret);                                              \
} while (0)

#define LOG_CHECK_ERR_RETURN(ret, ...)                      \
    do {                                                    \
    if (S_SUCCESS                    != ret &&              \
        E_DC_CONTENT_RETRY           != ret &&              \
        E_DC_CONTENT_OVER            != ret &&              \
        E_DC_TASK_MEM_VOPS_NOT_OVER  != ret ) {             \
        LOG(DC_COMMON_LOG_ERROR, "\t at");                  \
        return ret;                                         \
    }                                                       \
}while (0)

#define LOG_CHECK_ERR(ret) do {                             \
    if (S_SUCCESS          != ret &&                        \
        E_DC_CONTENT_RETRY != ret &&                        \
        E_DC_CONTENT_OVER  != ret) {                        \
        LOG(DC_COMMON_LOG_ERROR, "\t    at");               \
    }                                                       \
}while (0)

#endif /* !_DC_LOG_TRACE_LOG_H_ */
