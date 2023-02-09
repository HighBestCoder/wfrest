#include "dc_diff.h"
#include "dc_common_error.h"
#include "dc_common_log.h"
#include "dc_common_trace_log.h"
#include "dc_common_assert.h"
#include "cJSON.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <thread>
#include <algorithm>

static uint64_t
dc_diff_time_to_usec(struct timeval t)
{
    return (uint64_t)((uint64_t)t.tv_sec * 1000 * 1000 + t.tv_usec);
}

void
dc_diff_single_row_t::build(const uint32_t cols_nr,
                            const uint64_t *length,
                            const uint8_t **cols)
{
    DC_COMMON_ASSERT(cols_nr > 0);
    DC_COMMON_ASSERT(length != nullptr);
    DC_COMMON_ASSERT(cols != nullptr);
    r_cols_nr = cols_nr;

    uint32_t mem_bytes = 0;

    // memory cost by length
    mem_bytes += cols_nr * sizeof(uint64_t);
    // mem cost of cols ptr array
    mem_bytes += cols_nr * sizeof(uint8_t*);
    // memory cost by cols
    for (uint32_t i = 0; i < cols_nr; i++) {
        mem_bytes += length[i];
    }

    uint8_t *addr = nullptr;
    // malloc meory of mem_bytes
    mem_addr = (uint8_t*)malloc(mem_bytes);
    DC_COMMON_ASSERT(mem_addr != nullptr);
    addr = mem_addr;

    r_cols_nr = cols_nr;
    r_length = (uint64_t*)addr;
    addr += cols_nr * sizeof(uint64_t);
    memcpy(r_length, length, cols_nr * sizeof(uint64_t));

    // copy cols array
    r_cols = (uint8_t**)addr;
    addr += cols_nr * sizeof(uint8_t*);

    // copy cols
    for (uint32_t i = 0; i < cols_nr; i++) {
        if (cols[i] != NULL) {
            r_cols[i] = addr;
            memcpy(r_cols[i], cols[i], length[i]);
        } else {
            r_cols[i] = nullptr;
        }
        addr += length[i];
    }
}

static int
dc_diff_str_cmp(const uint8_t *a,
                const uint32_t alen,
                const uint8_t *b,
                const uint32_t blen)
{
    const size_t min_len = alen < blen ? alen : blen;
    if (a == b) {
        return alen == blen ? 0 : 1;
    }

    int r = strncmp((const char*)a, (const char *)b, min_len);

    if (r == 0) {
        if (alen < blen) {
            r = -1;
        } else if (alen > blen) {
            r = +1;
        }
    }

    return r;
}

void
dc_diff_single_row_t::build(const dc_diff_single_row_t *row,
                            const uint32_t cols_nr,
                            const uint64_t *length,
                            const uint8_t **cols) {
    DC_COMMON_ASSERT(row->r_cols_nr > 0);
    DC_COMMON_ASSERT(row->r_length != nullptr);
    DC_COMMON_ASSERT(row->r_cols != nullptr);
    DC_COMMON_ASSERT(cols_nr > 0);
    DC_COMMON_ASSERT(length != nullptr);
    DC_COMMON_ASSERT(cols != nullptr);
    DC_COMMON_ASSERT(cols_nr == row->r_cols_nr);
    bool col_is_same[cols_nr];

    r_cols_nr = row->r_cols_nr;

    int mem_bytes = 0;
    // memory cost by length
    mem_bytes += row->r_cols_nr * sizeof(uint64_t);

    // mem cost of cols array
    mem_bytes += row->r_cols_nr * sizeof(uint8_t*);

    // memory cost by cols content
    for (uint32_t i = 0; i < cols_nr; i++) {
        col_is_same[i] = false;
        if (cols[i] != nullptr && row->r_cols[i] != nullptr) {
            if (dc_diff_str_cmp(cols[i],
                                length[i],
                                row->r_cols[i],
                                row->r_length[i]) == 0) {
                // if the string is the same
                col_is_same[i] = true;
            }
        }

        if (!col_is_same[i]) {
            mem_bytes += length[i];
        }
    }

    uint8_t *addr = nullptr;
    // malloc meory of mem_bytes
    mem_addr = (uint8_t*)malloc(mem_bytes);
    DC_COMMON_ASSERT(mem_addr != nullptr);
    addr = mem_addr;

    r_cols_nr = row->r_cols_nr;
    r_length = (uint64_t*)addr;
    addr += row->r_cols_nr * sizeof(uint64_t);
    mem_bytes -= row->r_cols_nr * sizeof(uint64_t);
    memcpy(r_length, length, row->r_cols_nr * sizeof(uint64_t));

    // copy cols array
    r_cols = (uint8_t**)addr;
    addr += row->r_cols_nr * sizeof(uint8_t*);
    mem_bytes -= row->r_cols_nr * sizeof(uint8_t*);

    // copy cols
    for (uint32_t i = 0; i < cols_nr; i++) {
        // 如果是一样的，那么就指过去!
        if (col_is_same[i]) {
            r_cols[i] = row->r_cols[i];
        } else {
            // 否则不一样!
            if (cols[i] != NULL) {
                r_cols[i] = addr;
                memcpy(r_cols[i], cols[i], length[i]);
                addr += length[i];
                mem_bytes -= length[i];
            } else {
                r_cols[i] = nullptr;
            }
        }
    }

    DC_COMMON_ASSERT(mem_bytes == 0);
}

bool
dc_diff_single_row_t::is_same(const uint32_t cols_nr,
                              const uint64_t *length,
                              const uint8_t **cols) const {
    DC_COMMON_ASSERT(cols_nr > 0);
    DC_COMMON_ASSERT(length != nullptr);
    DC_COMMON_ASSERT(cols != nullptr);
    DC_COMMON_ASSERT(r_cols_nr == cols_nr);

    // 如果长度不一样，那么返回false
    if (memcmp(r_length, length, cols_nr * sizeof(uint64_t)) != 0) {
        return false;
    }

    for (uint32_t i = 0; i < cols_nr; i++) {
        if (r_cols[i] == nullptr && cols[i] == nullptr) {
            continue;
        }

        if (r_cols[i] == nullptr || cols[i] == nullptr) {
            return false;
        }

        if (dc_diff_str_cmp(r_cols[i],
                            r_length[i],
                            cols[i],
                            length[i]) != 0) {
            return false;
        }
    }

    return true;
}

std::string
dc_diff_single_row_t::to_string(void) const
{
    std::string str;
    for (uint32_t i = 0; i < r_cols_nr; i++) {
        if (i > 0) {
            str += " | ";
        }

        if (r_cols[i] == nullptr) {
            str += "NULL";
        } else {
            str += std::string((const char*)r_cols[i], r_length[i]);
        }
    }

    return str;
}

void
dc_diff_t::write_json_header(FILE *fp, dc_api_task_t *task)
{
    DC_COMMON_ASSERT(fp != nullptr);
    DC_COMMON_ASSERT(task != nullptr);

    fprintf(fp, "{\"uuid\":\"");
    fwrite(task->t_task_uuid.c_str(),
           task->t_task_uuid.length(),
           1,
           fp);
    fwrite("\",", 1, 2, fp);

    fprintf(fp, "\"rows\":["); // begin of row list.
}

static std::string
dc_compare_exe_time_to_str(struct timeval t)
{
    struct tm *tm_t;
    char time_str[128];
    int len;

    tm_t = localtime(&t.tv_sec);
    DC_COMMON_ASSERT(tm_t != NULL);

    len = snprintf(time_str,
             64,
             "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tm_t->tm_year+1900,
             tm_t->tm_mon+1, 
             tm_t->tm_mday,
             tm_t->tm_hour, 
             tm_t->tm_min, 
             tm_t->tm_sec,
             t.tv_usec / 1000);
    time_str[len] = 0;
    return std::string(time_str);
}

void
dc_diff_t::write_json_tail(FILE *fp, dc_api_task_t *task)
{
    DC_COMMON_ASSERT(fp != nullptr);
    DC_COMMON_ASSERT(task != nullptr);
}

dc_diff_hash_t::dc_diff_hash_t(dc_api_task_t *task) {
    DC_COMMON_ASSERT(task != nullptr);

    task_ = task;
    thd_write_down_ = std::thread(&dc_diff_hash_t::thd_write_down, this);

    json_buf_ = (uint8_t*)malloc(1024);
    json_buf_len_ = 1024;
}

dc_diff_hash_t::~dc_diff_hash_t() {
    // set thd to exit true
    thd_write_down_exit_ = true;
    thd_write_down_.join();
    task_ = nullptr;
    free(json_buf_);
    json_buf_len_  = 0;
}

int
dc_diff_row_t::find_same_row(const uint32_t cols_nr,
                             const uint64_t *length,
                             const uint8_t **cols)
{
    // find the same row in r_rows
    // if not found, return -1
    // if found, return the index of r_idxs
    for (uint32_t i = 0; i < r_rows.size(); i++) {
        auto &r = r_rows[i];
        if (r->is_same(cols_nr, length, cols)) {
            return i;
        }
    }

    return -1;
}

bool
dc_diff_row_t::has_full_with_rows(void) const
{
    return false;
}

bool
dc_diff_row_t::all_rows_are_same(void) const
{
    // 这个时候，应该是has_full_with_rows() == true;
    return r_rows.size() == 1;
}

dc_common_code_t
dc_diff_row_t::add_row(const int idx,
                       const uint32_t cols_nr,
                       const uint64_t *length,
                       const uint8_t **cols)
{
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(idx >= 0);
    DC_COMMON_ASSERT(idx < (int)r_task->t_server_info_arr.size());
    DC_COMMON_ASSERT(idx < r_idxs.size());

    // get server info from task_
    dc_api_ctx_default_server_info_t &server_info = r_task->t_server_info_arr[idx];
    int same_row_index = find_same_row(cols_nr, length, (const uint8_t**)cols);
    if (same_row_index != -1) {
        r_idxs[idx] = same_row_index;
    } else {
        dc_diff_single_row_t *cur_row = new dc_diff_single_row_t;
        if (r_rows.empty()) {
            cur_row->build(cols_nr, length, (const uint8_t**)cols);
        } else {
            DC_COMMON_ASSERT(r_task->t_std_idx >= 0);
            DC_COMMON_ASSERT(r_task->t_std_idx < r_task->t_server_info_arr.size());
            DC_COMMON_ASSERT(r_idxs.size() == r_task->t_server_info_arr.size());
            DC_COMMON_ASSERT(r_rows.size() > 0);

            // if we alread has std row, then we build rows with std_row
            const int std_server_idx = r_task->t_std_idx;
            DC_COMMON_ASSERT(std_server_idx < r_idxs.size() && std_server_idx >= 0);
            const int std_row_idx = r_idxs[std_server_idx];

            if (std_row_idx == -1) {
                // 如果还没有，那么我们就随便找一行!
                cur_row->build(r_rows[0], cols_nr, length, (const uint8_t**)cols);
            } else {
                DC_COMMON_ASSERT(std_row_idx < r_rows.size());
                cur_row->build(r_rows[std_row_idx], cols_nr, length, (const uint8_t**)cols);
            }
        }
        r_rows.push_back(cur_row);
        r_idxs[idx] = r_rows.size() - 1;
    }

    return ret;
}

dc_common_code_t
dc_diff_hash_t::add_row(const int idx,
                        const uint32_t cols_nr,
                        const uint64_t *length,
                        const uint8_t **cols)
{
    DC_COMMON_ASSERT(idx >= 0);
    DC_COMMON_ASSERT(idx < task_->t_server_info_arr.size());

    auto ret = do_add_to_hash(idx, cols_nr, length, cols);
    LOG_CHECK_ERR_RETURN(ret);

    return ret;
}

dc_common_code_t
dc_diff_hash_t::handle_failure(const int idx)
{
    DC_COMMON_ASSERT(idx >= 0);
    DC_COMMON_ASSERT(idx < task_->t_server_info_arr.size());

    auto ret = do_handle_failure(idx);

    return ret;
}

void
dc_diff_hash_t::thd_write_down()
{
    FILE *fp = nullptr;

    // open task uuid as filename
    fp = fopen(task_->t_task_uuid.c_str(), "w");
    DC_COMMON_ASSERT(fp != nullptr);

    write_json_header(fp, task_);

    while (!thd_write_down_exit_) {
        dc_diff_row_t *diff = pop_diff();
        if (diff == nullptr) {
            continue;
        }

        // 收到退出任务
        if (diff->r_exit_tag) {
            delete diff;
            break;
        }

        write_down(fp, diff);
        delete diff;
    }

    write_json_tail(fp, task_);
    fclose(fp);
    notify_write_content_over();
}

void
dc_diff_hash_t::send_diff(dc_diff_row_t *diff)
{
    std::unique_lock<std::mutex> lock(write_down_list_mutex_);
    write_down_list_.push_back(diff);
    write_down_list_cond_.notify_one();
}

dc_diff_row_t*
dc_diff_hash_t::pop_diff(void)
{
    dc_diff_row_t *diff = nullptr;
    std::unique_lock<std::mutex> lock(write_down_list_mutex_);

    write_down_list_cond_.wait_for(lock,
                                   std::chrono::milliseconds(1),
                                   [this] { return !write_down_list_.empty(); });

    if (!write_down_list_.empty()) {
        diff = write_down_list_.front();
        write_down_list_.pop_front();
    }

    return diff;
}

void
dc_diff_hash_t::write_down(FILE *fp, dc_diff_row_t *diff)
{

    dc_api_task_t *task = nullptr;

    DC_COMMON_ASSERT(diff != nullptr);
    DC_COMMON_ASSERT(diff->r_task != nullptr);
    DC_COMMON_ASSERT(fp != nullptr);

}

// 这里只是增加不同的行数
void
dc_diff_hash_t::write_down1(FILE *fp, dc_diff_row_t *diff)
{
    DC_COMMON_ASSERT(diff != nullptr);
    DC_COMMON_ASSERT(diff->r_task != nullptr);
    DC_COMMON_ASSERT(fp != nullptr);

    return;
}

// 这里会写入详细的信息
void
dc_diff_hash_t::write_down2(FILE *fp, dc_diff_row_t *diff)
{
    DC_COMMON_ASSERT(fp != NULL);
    DC_COMMON_ASSERT(diff != NULL);
    return;
}

dc_common_code_t
dc_diff_hash_t::gen_diff(dc_api_task_t *task)
{
    DC_COMMON_ASSERT(task != nullptr);

    do_flush_hash_map(task);

    // 生成一个退出信号
    dc_diff_row_t *diff_row = new dc_diff_row_t(0);
    diff_row->r_task = task_;
    diff_row->r_exit_tag = true;
    send_diff(diff_row);

    wait_write_content_over();

    return S_SUCCESS;
}

void
dc_diff_hash_t::wait_write_content_over()
{
    // wait thd write content over
    std::unique_lock<std::mutex> lock(thd_content_write_over_mutex_);
    thd_content_write_over_cond_.wait(lock, [this] { return thd_content_write_over_; });
}

void
dc_diff_hash_t::notify_write_content_over()
{
    std::unique_lock<std::mutex> lock(thd_content_write_over_mutex_);
    thd_content_write_over_ = true;
    thd_content_write_over_cond_.notify_one();

    LOG(DC_COMMON_LOG_INFO, "task:%s write content over", task_->t_task_uuid.c_str());
}

dc_diff_string_hash_t::dc_diff_string_hash_t(dc_api_task_t *task)
    : dc_diff_hash_t(task)
{
    hash_key_buf_ = (char*)malloc(512);
    hash_key_buf_len_ = 512;
}

dc_diff_string_hash_t::~dc_diff_string_hash_t()
{
    if (hash_key_buf_ != nullptr) {
        free(hash_key_buf_);
        hash_key_buf_ = nullptr;
    }
}

dc_common_code_t
dc_diff_string_hash_t::build_filter_rows(dc_api_task_t *task)
{
    DC_COMMON_ASSERT(task != nullptr);

    DC_COMMON_ASSERT(!task->t_server_info_arr.empty());
    dc_common_code_t ret = S_SUCCESS;

    return ret;
}

dc_common_code_t
dc_diff_string_hash_t::do_add_to_hash(const int idx,
                                      const uint32_t cols_nr,
                                      const uint64_t *length,
                                      const uint8_t **cols)
{
    dc_common_code_t ret = S_SUCCESS;

    return ret;
}

dc_common_code_t
dc_diff_string_hash_t::do_handle_failure(const int idx)
{
    dc_common_code_t ret = S_SUCCESS;

    return ret;
}

void
dc_diff_string_hash_t::do_flush_hash_map(dc_api_task_t *task)
{
    DC_COMMON_ASSERT(task != nullptr);

    return;
}

dc_common_code_t
dc_diff_int_hash_t::build_filter_rows(dc_api_task_t *task)
{
    dc_common_code_t ret = S_SUCCESS;

    return ret;
}

dc_common_code_t
dc_diff_int_hash_t::do_add_to_hash(const int idx,
                                   const uint32_t cols_nr,
                                   const uint64_t *length,
                                   const uint8_t **cols)
{
    dc_common_code_t ret = S_SUCCESS;

    return ret;
}

dc_common_code_t
dc_diff_int_hash_t::do_handle_failure(const int idx)
{
    dc_common_code_t ret = S_SUCCESS;

    return ret;
}

void
dc_diff_int_hash_t::do_flush_hash_map(dc_api_task_t *task)
{
    DC_COMMON_ASSERT(task != nullptr);

    return;
}
