#include "dc_diff_content.h"
#include "dc_common_log.h"
#include "dc_common_trace_log.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

dc_diff_content_t::dc_diff_content_t(dc_api_task_t *task) {
    task_ = task;

    row_buf_ = (uint8_t*) malloc(1024);
    row_buf_len_ = 1024;
}

dc_diff_content_t::~dc_diff_content_t() {
    if (row_buf_) {
        free(row_buf_);
        row_buf_ = nullptr;
        row_buf_len_ = 0;
    }
}

dc_common_code_t
dc_diff_content_t::add_row(const int idx,
                           const uint32_t cols_nr,
                           const uint64_t *length,
                           const uint8_t **cols) {
    DC_COMMON_ASSERT(!(idx < 0 || idx >= (int)task_->t_server_info_arr.size()));
    return S_SUCCESS;
}

dc_common_code_t
dc_diff_content_t::gen_diff(dc_api_task_t *task)
{
    DC_COMMON_ASSERT(task != nullptr);

    return S_SUCCESS;
}  // namespace dc

