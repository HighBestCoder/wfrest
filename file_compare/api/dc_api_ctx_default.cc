#include "dc_api_ctx_default.h"
#include "dc_common_assert.h"
#include "dc_common_trace_log.h"
#include "dc_common_log.h"
#include "dc_api_task.h"

#include "cJSON.h"

#include <stdlib.h>
#include <string.h>

dc_api_ctx_default_t::dc_api_ctx_default_t() : dc_api_ctx_t()
{
}

dc_api_ctx_default_t::~dc_api_ctx_default_t()
{
}

dc_common_code_t dc_api_ctx_default_t::perf()
{
    return S_SUCCESS;
}

dc_common_code_t
dc_api_ctx_default_t::task_add(const char *task_content,
                               const uint32_t task_content_len,
                               const int config_type)
{
    DC_COMMON_ASSERT(task_content != NULL);
    DC_COMMON_ASSERT(task_content_len > 0);
    DC_COMMON_ASSERT(config_type == DC_CONFIG_TYPE_JSON);

    // use cJSON to parse task_content
    cJSON *root = cJSON_Parse(task_content);
    if (root == NULL)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "cJSON_Parse failed, task_content: %.*s",
                     (int)task_content_len,
                     task_content);
        return E_ARG_INVALID;
    }

    auto task = new dc_api_task_t();
    if (task == NULL)
    {
        LOG_ROOT_ERR(E_ALLOC_FAILED,
                     "new dc_api_task_t failed, task_content: %.*s",
                     (int)task_content_len,
                     task_content);
        return E_OS_ENV_MEM;
    }

    auto ret = build_task_from_json(task_content,
                                    task_content_len,
                                    root,
                                    task);
    if (ret != S_SUCCESS)
    {
        cJSON_Delete(root);
        delete task;
    }
    LOG_CHECK_ERR_RETURN(ret);

    LOG(DC_COMMON_LOG_INFO, "task:%s added into compare queue", task->t_task_uuid.c_str());

    ret = compare_.add(task);
    if (ret != S_SUCCESS) {
        cJSON_Delete(root);
        delete task;
    }
    LOG_CHECK_ERR_RETURN(ret);

    // free root
    cJSON_Delete(root);

    return ret;
}

dc_common_code_t dc_api_ctx_default_t::task_start(const char *task_uuid, uint32_t task_uuid_len)
{
    if (task_uuid == NULL || task_uuid_len <= 0)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task_uuid is NULL or task_uuid_len <= 0");
        return E_ARG_INVALID;
    }

    auto ret = compare_.start(std::string(task_uuid, task_uuid_len));
    LOG_CHECK_ERR_RETURN(ret);

    return S_SUCCESS;
}

dc_common_code_t
dc_api_ctx_default_t::task_result(const char *task_uuid,
                                  uint32_t task_uuid_len,
                                  char *task_out_buf,
                                  uint32_t task_out_buf_len,
                                  int *result_bytes)
{
    if (task_uuid == NULL || task_uuid_len <= 0)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task_uuid is NULL or task_uuid_len <= 0");
        return E_ARG_INVALID;
    }

    auto ret = compare_.result(std::string(task_uuid, task_uuid_len),
                               task_out_buf,
                               task_out_buf_len,
                               result_bytes);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

dc_common_code_t
dc_api_ctx_default_t::task_cancel(const char *task_uuid,
                                  uint32_t task_uuid_len)
{
    if (task_uuid == NULL || task_uuid_len <= 0)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task_uuid is NULL or task_uuid_len <= 0");
        return E_ARG_INVALID;
    }

    auto ret = compare_.cancel(std::string(task_uuid, task_uuid_len));
    LOG_CHECK_ERR_RETURN(ret);

    return S_SUCCESS;
}