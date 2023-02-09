#ifndef _DC_DIFF_CONTENT_H_
#define _DC_DIFF_CONTENT_H_

#include "dc_diff.h"
#include "dc_api_task.h"

#include <list>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>

class dc_diff_content_t : public dc_diff_t {
public:
    dc_diff_content_t(dc_api_task_t *task);
    virtual ~dc_diff_content_t();

    virtual dc_common_code_t build_filter_rows(dc_api_task_t *task) override {
        // TODO check
        return S_SUCCESS;
    }

    virtual dc_common_code_t add_row(const int idx,
                                     const uint32_t cols_nr,
                                     const uint64_t *length,
                                     const uint8_t **cols) override;

    virtual dc_common_code_t handle_failure(const int idx) override {
        // TODO check
        return S_SUCCESS;
    }

    virtual dc_common_code_t gen_diff(dc_api_task_t *task) override;
private:
    dc_api_task_t *task_ { nullptr };

    uint8_t *row_buf_ { nullptr };
    uint32_t row_buf_len_ { 0 };
};

#endif /* _DC_DIFF_CONTENT_H_ */