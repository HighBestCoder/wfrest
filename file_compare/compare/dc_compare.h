#ifndef _DC_COMPARE_H_
#define _DC_COMPARE_H_

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "chan.h"
#include "dc_api_task.h"
#include "dc_common_error.h"
#include "dc_content.h"
#include "dc_diff.h"
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"
#include "workflow/WFFacilities.h"

class dc_compare_t {
public:
    dc_compare_t();
    ~dc_compare_t();

    dc_common_code_t add(dc_api_task_t *task);
    dc_common_code_t get(wfrest::Json &result_json /*OUT*/);

private:
    // 线程函数的入口main
    dc_common_code_t execute();

    // 文件比较函数
    dc_common_code_t exe_sql_job_for_file(dc_api_task_t *task, const char *file_path, bool is_dir,
                                          wfrest::Json &single_file_compare_result_json /*OUT*/);

    // 执行单个文件!
    dc_common_code_t exe_sql_job_for_single_item_file(dc_api_task_t *task, const char *file_rull_path,
                                                      const int json_idx, std::vector<dc_content_t *> &content_list);

    // 执行单个文件!
    dc_common_code_t exe_sql_job_for_single_item_dir(dc_api_task_t *task, const char *dir_full_path, const int json_idx,
                                                     std::vector<dc_content_t *> &content_list);

    // 目录比较函数
    dc_common_code_t exe_sql_job_for_dir(dc_api_task_t *task, const char *path, int depth,
                                         std::vector<dc_content_t *> &content_list);

    dc_common_code_t exe_sql_job1(dc_api_task_t *task, std::vector<dc_content_t *> &content_list);

    // 线程函数：执行一个任务
    dc_common_code_t exe_sql_job(dc_api_task_t *task);

private:
    std::vector<std::pair<std::string, int>> dir_q_;

private:
    std::atomic<bool> exit_;
    std::thread *worker_threads_{nullptr};

    task_chan_t task_q_;
    task_chan_t out_q_;
};

#endif /* ! _DC_COMPARE_H_ */