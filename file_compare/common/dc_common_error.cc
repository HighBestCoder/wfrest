#include "dc_common_error.h"            /* code_t           */
#include "dc_common_assert.h"           /* assert_t         */

static const char                       *TASK_NAME[] = {
    "T_TASK_STAT_TASK_STAT_ADDED_TO_TASK_QUEUE",
    "T_TASK_STAT_BEGIN_COMPARE",
    "T_TASK_STAT_COMPARE_OVER",
};

const char*
dc_common_error_task_status(uint32_t task_status)
{
    if (!(task_status < sizeof(TASK_NAME) / sizeof(*TASK_NAME))) {
        dc_common_panic("assert at %s(%s:%d)", __func__, __FILE__, __LINE__);
    }

    return TASK_NAME[task_status];
}

static const char *error_code_msg[42] = {
    "success",
    "function not implemented",
    "assert failed",

    "argument invalid", /* 无效参数          */

    "os env structure not found", /* OS ENV           */
    "os env memory error",
    "os env stat error",
    "os env open error",
    "os env read error",
    "os env write error",
    "os env unlink error",

    "dc api ctx not found", /* API             */
    "dc api ctx no free",

    "dc compare connect error",
    "dc compare exe task failed",
    "dc db module need retry", /* db module        */
    "dc db msql init error",
    "dc db msql set charset error",
    "dc db msql set connect timeout error",
    "dc db msql connect error",
    "dc db mysql query error",
    "dc db mysql use result error",
    "dc db mysql no result error",
    "dc db mysql fetch row error",
    "dc db mysql no more rows error",

    // 新加的
    "dc task has been canceled",
    "dc db mysql fetch field error",
    "dc db mysql pk not found error",
    "dc task too many diff rows",
};

const char *
dc_common_code_msg(dc_common_code_t error)
{
    error_code_msg[41] = "dc task operations not over";
    DC_COMMON_ASSERT(error < E_DC_COMMON_ERROR_END);
    return error_code_msg[error];
}