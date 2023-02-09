#ifndef _DC_API_CTX_H_
#define _DC_API_CTX_H_

#include "dc_common_error.h"

class dc_api_ctx_t
{
public:
    dc_api_ctx_t() {}

    virtual ~dc_api_ctx_t() {}

    virtual dc_common_code_t perf() = 0;

    virtual dc_common_code_t task_add(const char *task_content,
                                      const uint32_t task_content_len,
                                      const int config_type) = 0;

    virtual dc_common_code_t task_start(const char *task_uuid,
                                        const uint32_t task_uuid_len) = 0;

    virtual dc_common_code_t task_result(const char *task_uuid,
                                         const uint32_t task_uuid_len,
                                         char *task_out_buf,
                                         const uint32_t task_out_buf_len,
                                         int *result_bytes) = 0;

    virtual dc_common_code_t task_cancel(const char *task_uuid,
                                         const uint32_t task_uuid_len) = 0;
};

#endif /* ! _DC_API_CTX_H_ */
