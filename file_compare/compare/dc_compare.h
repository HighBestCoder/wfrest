#ifndef _DC_COMPARE_H_
#define _DC_COMPARE_H_

#include "dc_api_task.h"
#include "dc_common_error.h"
#include "dc_diff.h"
#include "dc_content.h"

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>

class dc_compare_t {
 public:
  dc_compare_t(const uint32_t thd_nr);
  ~dc_compare_t();

  dc_common_code_t add(dc_api_task_t *task);
  dc_common_code_t start(std::string task_uuid);
  dc_common_code_t cancel(std::string task_uuid);
  dc_common_code_t result(std::string task_uuid,
                          char *task_out_buf,
                          const uint32_t task_out_buf_len,
                          int *result_bytes);
 private:
  // 检查任务是否失败了!
  dc_common_code_t check_task_is_failed(dc_api_task_t* &task);

  // 线程函数的入口main
  dc_common_code_t execute(const int worker_id);

  // 线程函数：取一个任务
  dc_api_task_t *pop_one_task(const int worker_id);

  // 文件比较函数
  dc_common_code_t exe_sql_job_for_file(dc_api_task_t *task, const int worker_id, const char *file_path);

  // 目录比较函数
  dc_common_code_t exe_sql_job_for_dir(dc_api_task_t *task, const int worker_id, const char *dir_path);

  // 线程函数：执行一个任务
  dc_common_code_t exe_sql_job(dc_api_task_t *task, const int worker_id);

  // 线程函数：尝试free content_list
  dc_common_code_t try_free_content_list(int worker_id);

 private:
  std::atomic<bool> exit_;
  std::vector<std::thread> thd_list_;

  /*
   * 这里需要说明一下task_list_与task_status_list_的关系。
   *
   * 首先，有X个worker，就会有X个线程，那么就会有:
   *
   * - X把锁
   * - X个task_list
   * - X个task_status_list
   *
   * 首先(对于单个worker/线程来说)：
   * 1. task_status_list是一个哈希，进来的task刚开始总是扔到这里!并且处于: init状态。
   *
   * 2. 当发起start之后。会把状态改成TO_RUN然后添加到task_list中。
   *    此时的生命周期就是由task_list管理。
   *
   * 3. 比如遇到cancel，在执行过程中会根据不同的阶段来进行任务的取消。
   *    那么一旦是在start之后，执行任务中发生，要销毁这些任务的话，都是由
   *    task_list来管理的。
   *
   * 4. 但是，如果是task_over，那么生命周期就扔回到了hash表task_status_list_里面。
   *
   */
  std::vector<std::mutex> task_mutex_list_;
  std::vector<std::condition_variable> task_cond_list_;
  std::vector<std::list<dc_api_task_t *>> task_list_;
  std::vector<std::unordered_map<std::string, dc_api_task_t*>> task_status_list_;
  std::vector<std::list<dc_content_t *>> task_content_list_;
};

#endif /* ! _DC_COMPARE_H_ */