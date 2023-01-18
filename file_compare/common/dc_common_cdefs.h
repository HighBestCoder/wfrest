#ifndef _DC_COMMON_CDEFS_H_
#define _DC_COMMON_CDEFS_H_

#ifdef __cplusplus
    #define __BEGIN_DECLS               extern "C" {
    #define __END_DECLS                 }
    #define CTASSERT                    static_assert
#else
    #define __BEGIN_DECLS
    #define __END_DECLS
    #define CTASSERT                    _Static_assert
#endif

#ifndef __dead2
    #define __dead2                     __attribute__((__noreturn__))
#endif

#ifndef __aligned
    #define __aligned(x)                __attribute__((__aligned__(x)))
#endif

#ifndef __dc_unused
    #define __dc_unused                    __attribute__((__unused__))
#endif

#ifndef __printflike
    #define __printflike(f, s)          __attribute__((__format__(printf, f, s)))
#endif

#ifndef __packed
    #define __packed                    __attribute__((__packed__))
#endif

#ifndef CACHELINE_SIZE
    #define CACHELINE_SIZE              64
#endif

#ifndef __cachealigned
    #define __cachealigned              __aligned(CACHELINE_SIZE)
#endif

#ifndef __predict_true
    #define __predict_true(exp)         __builtin_expect((exp), 1)
#endif

#ifndef __predict_false
    #define __predict_false(exp)        __builtin_expect((exp), 0)
#endif

#ifndef __DECONST
    #define __DECONST(type, var)        ((type)(uintptr_t)(const void *)(var))
#endif

#endif	/* !_DC_COMMON_CDEFS_H_ */
