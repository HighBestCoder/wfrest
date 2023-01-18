#ifndef _DC_COMMON_ARG_H_
#define _DC_COMMON_ARG_H_

#ifndef __DECONST
#define __DECONST(type, var) ((type)(uintptr_t)(const void *)(var))
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef NULL
#define NULL 0
#endif

enum
{
    DC_CONFIG_TYPE_BEGIN = 0,

    /* 新加类型, 不要改变下面的顺序      */
    DC_CONFIG_TYPE_JSON = 0,
    DC_CONFIG_TYPE_YAML,
    DC_CONFIG_TYPE_INI,
    DC_CONFIG_TYPE_CSTR,

    /* 在这里添加新类型                */

    DC_CONFIG_TYPE_INVALID, /*LAST! DO NOT MOVE*/
};

#endif /* ! _DC_COMMON_ARG_H_ */
