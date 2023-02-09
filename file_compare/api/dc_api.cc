#include "dc_api.h"                     /* dc_api_XXX       */
#include "dc_common_error.h"            /* DC_CONFIG_TYPE_  */
#include "dc_common_assert.h"           /* DC_COMMON_ASSERT */
#include "dc_common_trace_log.h"        /* LOG_XXX MACRO    */

#include "dc_api_ctx.h"                 /* api_ctx_t        */
#include "dc_api_ctx_default.h"

#include <mutex>                        /* mutex_t          */

#define DC_API_CTX_MAX_NR 8
#define DC_API_CTX_IDX_INVALID -1

static dc_api_ctx_t *CTX_ARR[DC_API_CTX_MAX_NR];
static std::mutex CTX_ARR_LOCK;

#ifdef __cplusplus
extern "C"
{
#endif

static dc_common_code_t
dc_api_ctx_construct2_locked(dc_api_ctx_idx_t *ctx_idx /*OUT*/)
{
    int i;
    dc_api_ctx_t *ctx;
    int free_idx;

    DC_COMMON_ASSERT(ctx_idx != NULL);

    free_idx = DC_API_CTX_IDX_INVALID;
    for (i = 0; i < DC_API_CTX_MAX_NR; i++)
    {
        ctx = CTX_ARR[i];
        if (ctx == NULL)
        {
            free_idx = i;
            break;
        }
    }

    if (free_idx == DC_API_CTX_IDX_INVALID)
    {
        LOG_ROOT_ERR(E_DC_API_CTX_NO_FREE,
                     "no free ctx for build");
        return E_DC_API_CTX_NO_FREE;
    }

    DC_COMMON_ASSERT(0 <= free_idx && free_idx < DC_API_CTX_MAX_NR);

    CTX_ARR[free_idx] = new dc_api_ctx_default_t();

    DC_COMMON_ASSERT(CTX_ARR[free_idx] != NULL);
    *ctx_idx = free_idx;

    return S_SUCCESS;
}

static dc_common_code_t
dc_api_ctx_construct1(dc_api_ctx_idx_t *ctx_idx /*OUT*/)
{
    dc_common_code_t ret;
    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);

    DC_COMMON_ASSERT(ctx_idx != NULL);

    ret = dc_api_ctx_construct2_locked(ctx_idx);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

dc_common_code_t
dc_api_ctx_construct(dc_api_ctx_idx_t *ctx_idx /*OUT*/)   /* 比较服务的索引   */
{
    dc_common_code_t ret;

    if (ctx_idx == NULL)
    {
        LOG_ROOT_ERR(E_ARG_INVALID, "ctx_idx:(%p) is NULL", ctx_idx);
        return E_ARG_INVALID;
    }

    *ctx_idx = DC_API_CTX_IDX_INVALID;
    ret = dc_api_ctx_construct1(ctx_idx);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

static dc_common_code_t
dc_api_ctx_destroy1_locked(dc_api_ctx_idx_t *ctx_idx /*CHANGED*/)
{
    dc_common_code_t ret;
    dc_api_ctx_t *ctx;

    DC_COMMON_ASSERT(ctx_idx != NULL);
    DC_COMMON_ASSERT(0 <= *ctx_idx);
    DC_COMMON_ASSERT(*ctx_idx < DC_API_CTX_MAX_NR);

    ctx = CTX_ARR[*ctx_idx];

    if (ctx == NULL)
    {
        LOG_ROOT_ERR(E_DC_API_CTX_NOT_FOUND,
                     "ctx_id:(%d) is not exist",
                     *ctx_idx);
        return E_DC_API_CTX_NOT_FOUND;
    }

    delete ctx;
    CTX_ARR[*ctx_idx] = NULL;

    return S_SUCCESS;
}

dc_common_code_t
dc_api_ctx_destroy(dc_api_ctx_idx_t *ctx_idx /*CHANGED*/)
{
    dc_common_code_t ret;

    if (ctx_idx == NULL)
    {
        LOG_ROOT_ERR(E_ARG_INVALID, "ctx is NULL");
        return E_ARG_INVALID;
    }

    if (!(0 <= *ctx_idx && *ctx_idx < DC_API_CTX_MAX_NR))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "ctx_id:(%d) is invalid",
                     *ctx_idx);
        return E_ARG_INVALID;
    }

    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);
    ret = dc_api_ctx_destroy1_locked(ctx_idx);
    LOG_CHECK_ERR_RETURN(ret);

    *ctx_idx = DC_API_CTX_IDX_INVALID;

    return ret;
}

static dc_common_code_t
dc_api_auth_add1_locked(dc_api_ctx_idx_t ctx_idx,
                        const char *auth_info,
                        const uint32_t auth_info_len,
                        const int auth_type)
{
    dc_api_ctx_t *ctx;
    dc_common_code_t ret;

    return E_NOT_IMPL;
}

dc_common_code_t
dc_api_auth_add(dc_api_ctx_idx_t ctx_idx,
                const char *auth_info,
                const uint32_t auth_info_len,
                const int auth_type)
{
    dc_common_code_t ret;

    if (!(0 <= ctx_idx &&
          ctx_idx < DC_API_CTX_MAX_NR))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "ctx_idx:(%d) is invalid",
                     ctx_idx);
        return E_ARG_INVALID;
    }

    if (auth_info == NULL ||
        auth_info_len == 0 ||
        auth_info_len > PATH_MAX)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "auth_info:(%p) "
                     "auth_info_len:(%u) "
                     "arg invalid",
                     auth_info,
                     auth_info_len);
        return E_ARG_INVALID;
    }

    if (!(DC_CONFIG_TYPE_BEGIN <= auth_type &&
          auth_type < DC_API_CTX_IDX_INVALID))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "auth_type:(%d) is invalid");
        return E_ARG_INVALID;
    }

    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);
    ret = dc_api_auth_add1_locked(ctx_idx,
                                  auth_info,
                                  auth_info_len,
                                  auth_type);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

static dc_common_code_t
dc_api_auth_get1_locked(dc_api_ctx_idx_t ctx_idx,
                        const char *auth_name,
                        const uint32_t auth_name_len,
                        const int auth_type,
                        char *out_buf,
                        const uint32_t out_buf_len,
                        int *auth_info_bytes /*OUT*/)
{
    dc_common_code_t ret;
    dc_api_ctx_t *ctx;

    /*
     * HINT:
     * 1. out_buf 可以为空
     * 2. out_buf_len可以为0
     *
     * 如果为1\2，那么
     * 返回auth_info_bytes
     * 告诉调用方需要多少buffer.
     */
    DC_COMMON_ASSERT(ctx_idx >= 0 &&
                     ctx_idx < DC_API_CTX_MAX_NR &&
                     auth_name != NULL &&
                     auth_name_len > 0 &&
                     auth_name_len < PATH_MAX &&
                     auth_type >= DC_CONFIG_TYPE_BEGIN &&
                     auth_type < DC_CONFIG_TYPE_INVALID);

    ctx = CTX_ARR[ctx_idx];

    DC_COMMON_ASSERT(ctx != NULL);

    return E_NOT_IMPL;
}

dc_common_code_t
dc_api_auth_get(dc_api_ctx_idx_t ctx_idx,
                const char *auth_name,
                const uint32_t auth_name_len,
                const int auth_type,
                char *out_buf,
                const uint32_t out_buf_len,
                int *auth_info_bytes /*OUT*/)
{
    dc_common_code_t ret;

    if (!(0 <= ctx_idx &&
          ctx_idx < DC_API_CTX_MAX_NR))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "ctx_idx:(%d) is invalid",
                     ctx_idx);
        return E_ARG_INVALID;
    }

    if (auth_name == NULL ||
        auth_name_len == 0 ||
        auth_name_len > PATH_MAX)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "auth_name:(%p) "
                     "auth_name_len:(%u) "
                     "arg invalid",
                     auth_name,
                     auth_name_len);
        return E_ARG_INVALID;
    }

    if (auth_info_bytes == NULL)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "auth_info_bytes ptr is NULL");
        return E_ARG_INVALID;
    }

    if (!(DC_CONFIG_TYPE_BEGIN <= auth_type &&
          auth_type < DC_CONFIG_TYPE_INVALID))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "auth_type:(%d) is invalid in range[%d ~ %d)",
                     auth_type,
                     DC_CONFIG_TYPE_BEGIN,
                     DC_CONFIG_TYPE_INVALID);
        return E_ARG_INVALID;
    }

    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);
    ret = dc_api_auth_get1_locked(ctx_idx,
                                  auth_name,
                                  auth_name_len,
                                  auth_type,
                                  out_buf,
                                  out_buf_len,
                                  auth_info_bytes);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

static dc_common_code_t
dc_api_auth_delete1_locked(dc_api_ctx_idx_t ctx_idx,
                           const char *auth_name,
                           const uint32_t auth_name_len)
{
    dc_common_code_t ret;
    dc_api_ctx_t *ctx;

    DC_COMMON_ASSERT(ctx_idx >= 0 &&
                     ctx_idx < DC_API_CTX_MAX_NR &&
                     auth_name != NULL &&
                     auth_name_len > 0 &&
                     auth_name_len < PATH_MAX);
    return E_NOT_IMPL;
}

dc_common_code_t
dc_api_auth_delete(dc_api_ctx_idx_t ctx_idx,
                   const char *auth_name,
                   const uint32_t auth_name_len)
{
    dc_common_code_t ret;

    if (!(0 <= ctx_idx &&
          ctx_idx < DC_API_CTX_MAX_NR))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "ctx_idx:(%d) is invalid",
                     ctx_idx);
        return E_ARG_INVALID;
    }

    if (auth_name == NULL ||
        auth_name_len == 0 ||
        auth_name_len > PATH_MAX)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "auth_name:(%p) "
                     "auth_name_len:(%u) "
                     "arg invalid",
                     auth_name,
                     auth_name_len);
        return E_ARG_INVALID;
    }

    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);
    ret = dc_api_auth_delete1_locked(ctx_idx,
                                     auth_name,
                                     auth_name_len);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

static dc_common_code_t
dc_api_task_add1_locked(dc_api_ctx_idx_t ctx_idx,
                        const char *task_content,
                        const uint32_t task_content_len,
                        const int task_content_type)
{
    dc_common_code_t ret;
    dc_api_ctx_t *ctx;

    const int MAX_TASK_CONTENT_LEN = 10 * 1048576; // 10M

    DC_COMMON_ASSERT(ctx_idx >= 0);
    DC_COMMON_ASSERT(ctx_idx < DC_API_CTX_MAX_NR);
    DC_COMMON_ASSERT(task_content != NULL);
    DC_COMMON_ASSERT(task_content_len < MAX_TASK_CONTENT_LEN);
    DC_COMMON_ASSERT(task_content_type >= DC_CONFIG_TYPE_BEGIN);
    DC_COMMON_ASSERT(task_content_type < DC_CONFIG_TYPE_INVALID);

    ctx = CTX_ARR[ctx_idx];

    DC_COMMON_ASSERT(ctx != NULL);

    ret = ctx->task_add(task_content,
                        task_content_len,
                        task_content_type);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

dc_common_code_t
dc_api_task_add(dc_api_ctx_idx_t ctx_idx,
                const char *task_content,
                const uint32_t task_content_len,
                const int task_content_type)
{
    dc_common_code_t ret;

    if (!(0 <= ctx_idx &&
          ctx_idx < DC_API_CTX_MAX_NR))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "ctx_idx:(%d) is invalid",
                     ctx_idx);
        return E_ARG_INVALID;
    }

    if (task_content == NULL ||
        task_content_len == 0 ||
        task_content_len > PATH_MAX)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task_content:(%p) "
                     "task_content_len:(%u) "
                     "arg invalid",
                     task_content,
                     task_content_len);
        return E_ARG_INVALID;
    }

    if (!(DC_CONFIG_TYPE_BEGIN <= task_content_type &&
          task_content_type < DC_CONFIG_TYPE_INVALID))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task_content_type:(%d) is invalid",
                     task_content_type);
        return E_ARG_INVALID;
    }

    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);
    ret = dc_api_task_add1_locked(ctx_idx,
                                  task_content,
                                  task_content_len,
                                  task_content_type);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

static dc_common_code_t
dc_api_task_start1_locked(dc_api_ctx_idx_t ctx_idx,
                          const char *task_uuid,
                          const uint32_t task_uuid_len)
{
    dc_common_code_t ret;
    dc_api_ctx_t *ctx;

    DC_COMMON_ASSERT(ctx_idx >= 0 &&
                     ctx_idx < DC_API_CTX_MAX_NR &&
                     task_uuid != NULL &&
                     task_uuid_len > 0 &&
                     task_uuid_len < PATH_MAX);

    ctx = CTX_ARR[ctx_idx];
    ret = ctx->task_start(task_uuid,
                          task_uuid_len);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

dc_common_code_t
dc_api_task_start(dc_api_ctx_idx_t ctx_idx,
                  const char *task_uuid,
                  const uint32_t task_uuid_len)
{
    dc_common_code_t ret;

    if (!(0 <= ctx_idx &&
          ctx_idx < DC_API_CTX_MAX_NR))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "ctx_idx:(%d) is invalid",
                     ctx_idx);
        return E_ARG_INVALID;
    }

    if (task_uuid == NULL ||
        task_uuid_len == 0 ||
        task_uuid_len > PATH_MAX)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task_uuid:(%p) "
                     "task_uuid_len:(%u) "
                     "arg invalid",
                     task_uuid,
                     task_uuid_len);
        return E_ARG_INVALID;
    }

    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);
    ret = dc_api_task_start1_locked(ctx_idx,
                                    task_uuid,
                                    task_uuid_len);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

static dc_common_code_t
dc_api_task_get_result1_locked(dc_api_ctx_idx_t ctx_idx,
                               const char *task_uuid,
                               const uint32_t task_uuid_len,
                               const int out_format,
                               char *task_out_buf /*NULLable*/,
                               const uint32_t task_out_buf_len /* canbe 0*/,
                               int *result_bytes /*OUT*/)
{

    dc_common_code_t ret;
    dc_api_ctx_t *ctx;

    DC_COMMON_ASSERT(ctx_idx >= 0 &&
                     ctx_idx < DC_API_CTX_MAX_NR &&
                     task_uuid != NULL &&
                     task_uuid_len > 0 &&
                     task_uuid_len < PATH_MAX &&
                     out_format >= DC_CONFIG_TYPE_BEGIN &&
                     out_format < DC_CONFIG_TYPE_INVALID &&
                     result_bytes != NULL);

    ctx = CTX_ARR[ctx_idx];
    ret = ctx->task_result(task_uuid,
                           task_uuid_len,
                           task_out_buf,
                           task_out_buf_len,
                           result_bytes);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

dc_common_code_t
dc_api_task_get_result(dc_api_ctx_idx_t ctx_idx,
                       const char *task_uuid,
                       const uint32_t task_uuid_len,
                       const int out_format,
                       char *task_out_buf, /*nullable*/
                       const uint32_t task_out_buf_len /*maybe 0*/,
                       int *result_bytes /*OUT*/)
{
    dc_common_code_t ret;

    if (!(0 <= ctx_idx &&
          ctx_idx < DC_API_CTX_MAX_NR))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "ctx_idx:(%d) is invalid",
                     ctx_idx);
        return E_ARG_INVALID;
    }

    if (task_uuid == NULL ||
        task_uuid_len == 0 ||
        task_uuid_len > PATH_MAX)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task_uuid:(%p) "
                     "task_uuid_len:(%u) "
                     "arg invalid",
                     task_uuid,
                     task_uuid_len);
        return E_ARG_INVALID;
    }

    /* !注意!
     *
     * 传进来的参数里面
     *
     * - task_out_buf     可以为空
     * - task_out_buf_len 长度可以为0
     *
     */
    if (result_bytes == NULL)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "result_bytes is NULL");
        return E_ARG_INVALID;
    }

    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);
    ret = dc_api_task_get_result1_locked(ctx_idx,
                                         task_uuid,
                                         task_uuid_len,
                                         out_format,
                                         task_out_buf,
                                         task_out_buf_len,
                                         result_bytes);
    if (ret != E_DC_TASK_MEM_VOPS_NOT_OVER) {
        LOG_CHECK_ERR(ret);
    }

    return ret;
}

static dc_common_code_t
dc_api_task_cancel1_locked(dc_api_ctx_idx_t ctx_idx,
                           const char *task_uuid,
                           const uint32_t task_uuid_len)
{
    dc_common_code_t ret;
    dc_api_ctx_t *ctx;

    DC_COMMON_ASSERT(ctx_idx >= 0 &&
                     ctx_idx < DC_API_CTX_MAX_NR &&
                     task_uuid != NULL &&
                     task_uuid_len > 0 &&
                     task_uuid_len < PATH_MAX);

    ctx = CTX_ARR[ctx_idx];

    DC_COMMON_ASSERT(ctx != NULL);

    ret = ctx->task_cancel(task_uuid,
                           task_uuid_len);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

dc_common_code_t
dc_api_task_cancel(dc_api_ctx_idx_t ctx_idx,
                   const char *task_uuid,
                   const uint32_t task_uuid_len)
{
    dc_common_code_t ret;

    if (!(0 <= ctx_idx &&
          ctx_idx < DC_API_CTX_MAX_NR))
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "ctx_idx:(%d) is invalid",
                     ctx_idx);
        return E_ARG_INVALID;
    }

    LOG(DC_COMMON_LOG_INFO,
        "dc_api_task_cancel ctx_idx:(%d) "
        "task_uuid:(%s) "
        "task_uuid_len:(%u)",
        ctx_idx,
        task_uuid,
        task_uuid_len);

    if (task_uuid == NULL ||
        task_uuid_len == 0 ||
        task_uuid_len > PATH_MAX)
    {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task_uuid:(%p) "
                     "task_uuid_len:(%u) "
                     "arg invalid",
                     task_uuid,
                     task_uuid_len);
        return E_ARG_INVALID;
    }

    std::unique_lock<std::mutex> lock(CTX_ARR_LOCK);
    ret = dc_api_task_cancel1_locked(ctx_idx,
                                     task_uuid,
                                     task_uuid_len);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}
#ifdef __cplusplus
}
#endif
