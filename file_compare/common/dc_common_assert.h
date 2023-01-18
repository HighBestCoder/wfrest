#ifndef _DC_COMMON_ASSERT_H_
#define _DC_COMMON_ASSERT_H_

#include "dc_common_cdefs.h"
#include "dc_common_trace_log.h"

#ifndef DC_COMMON_NO_INVARIANTS         /* NO DC_COMMON_NO_INVARIANTS    */
    #define DC_COMMON_ASSERTMSG(exp, fmt, ...)              \
        do {                                                \
            if (__predict_false(!(exp)))                    \
                dc_common_panic(fmt, ##__VA_ARGS__);        \
        } while (0)

    #define DC_COMMON_ASSERT1(exp)                           \
        do {                                                \
            if (__predict_false(!(exp))) {                  \
                dc_common_panic("assert! %s(%s:%d)",        \
                                __func__,                   \
                                __FILE__,                   \
                                __LINE__);                  \
            }                                               \
        } while (0)

    #define DC_COMMON_ASSERT2(exp)                          \
        do {                                                \
            if (__predict_false(!(exp))) {                  \
                LOG_ROOT_ERR(E_ASSERT, "assert failed");    \
                return E_ASSERT;                            \
            }                                               \
        } while (0)

    #define DC_COMMON_ASSERT(exp)            DC_COMMON_ASSERT1(exp)
    #define DC_TEST_ASSERT(exp)              DC_COMMON_ASSERT1(exp)
#else                                   /* DC_COMMON_NO_INVARIANTS         */

    #define DC_COMMON_ASSERTMSG(exp, msg, ...)    do {} while (0)
    #define DC_COMMON_ASSERT(exp)				    do {} while (0)

#endif                                  /* !DC_COMMON_NO_INVARIANTS */

void
dc_common_panic(const char *fmt, ...) __printflike(1, 2) __dead2;

#endif	/* !_DC_COMMON_ASSERT_H_ */
