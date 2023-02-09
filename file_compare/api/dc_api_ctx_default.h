#ifndef _DC_API_CTX_DEFAULT_H_
#define _DC_API_CTX_DEFAULT_H_

#include "dc_api_ctx.h"
#include "dc_api_task.h"
#include "cJSON.h"
#include "dc_compare.h"

#include <unordered_map>
#include <mutex>
#include <vector>
#include <string>

class dc_api_ctx_default_t : public dc_api_ctx_t
{
public:
    dc_api_ctx_default_t();

    virtual ~dc_api_ctx_default_t();

    virtual dc_common_code_t perf() override;

    virtual dc_common_code_t task_add(const char *task_content,
                                      const uint32_t task_content_len,
                                      const int config_type) override;

    virtual dc_common_code_t task_start(const char *task_uuid,
                                        const uint32_t task_uuid_len) override;

    virtual dc_common_code_t task_result(const char *task_uuid,
                                         const uint32_t task_uuid_len,
                                         char *task_out_buf,
                                         const uint32_t task_out_buf_len,
                                         int *result_bytes) override;

    virtual dc_common_code_t task_cancel(const char *task_uuid,
                                         const uint32_t task_uuid_len) override;
private:
    // 比较器
    dc_compare_t compare_{4};
};

#endif /* _DC_API_CTX_DEFAULT_H_ */