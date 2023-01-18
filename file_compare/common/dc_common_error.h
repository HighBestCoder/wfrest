#ifndef _DC_COMMON_ERROR_H_
#define _DC_COMMON_ERROR_H_

#include "dc_common_arg.h" /* 包含通用设定       */

#include <stdint.h> /* uint8_t          */

#define TEST_BEGIN()                                            \
    do                                                          \
    {                                                           \
        printf("\033[40;34m%-40s\033[0m\t\tstart\n", __func__); \
    } while (0)

#define TEST_END()                                                               \
    do                                                                           \
    {                                                                            \
        printf("\033[40;34m%-40s\033[0m\t\t[\033[40;32mOK\033[0m]\n", __func__); \
    } while (0)

#ifndef __print_info__
#define __print_info__ __func__, __FILE__, __LINE__
#endif

/* code_t 表示错误码
 * - 0    : 表示成功
 * - other: 表示失败
 */
typedef enum dc_common_code
{
    S_SUCCESS = 0,

    E_NOT_IMPL,
    E_ASSERT,

    E_ARG_INVALID, /* 无效参数          */

    E_OS_ENV_NOT_FOUND, /* OS ENV           */
    E_OS_ENV_MEM,
    E_OS_ENV_STAT,
    E_OS_ENV_OPEN,
    E_OS_ENV_READ,
    E_OS_ENV_WRITE,
    E_OS_ENV_UNLINK,

    E_DC_API_CTX_NOT_FOUND, /* API             */
    E_DC_API_CTX_NO_FREE,

    E_DC_COMPARE_CONNECT,
    E_DC_COMPARE_EXE_TASK_FAILED,
    E_DC_DB_RETRY, /* db module        */
    E_DC_DB_MSQL_INIT,
    E_DC_DB_MSQL_SET_CHARSET,
    E_DC_DB_MSQL_SET_CONNECT_TIMEOUT,
    E_DC_DB_MSQL_CONNECT,
    E_DC_DB_MYSQL_QUERY,
    E_DC_DB_MYSQL_USE_RESULT,
    E_DC_DB_MYSQL_NO_RESULT,
    E_DC_DB_MYSQL_FETCH_ROW,
    E_DC_DB_MYSQL_FREE_RESULT,
    E_DC_DB_MYSQL_NO_MORE_ROWS,

    // 新加的
    E_DC_TASK_HAS_BEEN_CANCELED,
    E_DC_DB_MYSQL_FETCH_FIELD,
    E_DC_DB_MYSQL_PK_NOT_FOUND,
    E_DC_DIFF_TOO_MANY_DIFF_ROWS,

    E_DC_TASK_MEM_VOPS_NOT_OVER = 41,
    // END
    E_DC_COMMON_ERROR_END,
} dc_common_code_t;

typedef enum dc_common_task_stat
{
    T_TASK_INIT = 0,

    // 添加了到比较任务里面!
    T_TASK_TO_RUN,

    // 发状给compare之后，只有这三种状态
    T_TASK_RUNNING,
    T_TASK_OVER,
    T_TASK_CANCEL,

    /* 其他中间状态                        */
    T_TASK_INVALID,
} dc_common_task_stat_t;

const char *
dc_common_error_task_status(uint32_t task_status);

/*
 * 为了应对各种形式的输入，比如json/yaml/ini/cstr
 * 这些字符串。以及字符串中可能存在的各种编码
 * 这里先定义一下dc_text
 *
 * TODO 后面把char || len || type这些参数都换成这个。
 */

typedef struct dc_text
{
    uint8_t *t_data;
    uint32_t t_len;
    int t_format;
} dc_text_t;

/*
 * 通过code_t查询到具体的出错信息
 */
const char *
dc_common_code_msg(dc_common_code_t error);

#endif /* !_DC_COMMON_ERROR_H_ */
