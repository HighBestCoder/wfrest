#include "dc_compare.h"
#include "dc_common_assert.h"
#include "dc_common_error.h"
#include "dc_diff.h"
#include "dc_diff_content.h"
#include "dc_diff_failed_content.h"

#include "WFTaskFactory.h"

#include <stdlib.h>
#include <string.h>
#include <vector>
#include <functional>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

dc_compare_t::dc_compare_t(const uint32_t thd_nr)
    : task_mutex_list_(thd_nr),
      task_cond_list_(thd_nr),
      task_list_(thd_nr),
      task_status_list_(thd_nr),
      task_db_list_(thd_nr) {
    exit_ = false;
    DC_COMMON_ASSERT(thd_nr);
    for (uint32_t i = 0; i < thd_nr; i++) {
        thd_list_.emplace_back(&dc_compare_t::execute, this, i);
    }
}

dc_compare_t::~dc_compare_t() {
    exit_ = true;

    for (auto &thd : thd_list_) {
        thd.join();
    }

    // clean task list
    for (uint32_t i = 0; i < task_status_list_.size(); i++) {
        std::unique_lock<std::mutex> lock(task_mutex_list_[i]);
        for (auto &task : task_status_list_[i]) {
            delete task.second;
        }
    }
}

// 如果添加成功，那么接过生命周期
dc_common_code_t dc_compare_t::add(dc_api_task_t *task) {
    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(!task->t_task_uuid.empty());
    DC_COMMON_ASSERT(thd_list_.size() > 0);

    // hash task->t_task_uuid and get worker_id
    uint32_t worker_id = std::hash<std::string> {}(task->t_task_uuid) % thd_list_.size();

    DC_COMMON_ASSERT(worker_id < task_mutex_list_.size());
    std::lock_guard<std::mutex> lock(task_mutex_list_[worker_id]);

    DC_COMMON_ASSERT(worker_id < task_status_list_.size());
    // 看一下是不是已经存在了
    if (task_status_list_[worker_id].find(task->t_task_uuid) != task_status_list_[worker_id].end()) {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task:%p task:%s already exist in worker:%d",
                     task,
                     task->t_task_uuid.c_str(),
                     worker_id);
        return E_ARG_INVALID;
    }

    task_status_list_[worker_id][task->t_task_uuid] = task;
    DC_COMMON_ASSERT(task->t_task_status == T_TASK_INIT);

    LOG(DC_COMMON_LOG_INFO,
        "task:%s add to worker:%d",
        task->t_task_uuid.c_str(),
        worker_id);

    return S_SUCCESS;
}

dc_common_code_t
dc_compare_t::start(std::string task_uuid) {
    DC_COMMON_ASSERT(!task_uuid.empty());
    DC_COMMON_ASSERT(thd_list_.size() > 0);

    // hash task->t_task_uuid and get worker_id
    uint32_t worker_id = std::hash<std::string> {}(task_uuid) % thd_list_.size();

    std::lock_guard<std::mutex> lock(task_mutex_list_[worker_id]);

    // 看一下是不是已经存在了
    auto iter = task_status_list_[worker_id].find(task_uuid);
    if (iter == task_status_list_[worker_id].end()) {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task:%s not exist in worker:%d",
                     task_uuid.c_str(),
                     worker_id);
        return E_ARG_INVALID;
    }

    auto task = iter->second;
    DC_COMMON_ASSERT(task != nullptr);
    if (task->t_task_status != T_TASK_INIT) {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task:%s status:%d not init_status in worker:%d, status:%d"
                     ,
                     task_uuid.c_str(),
                     iter->second->t_task_status.load(std::memory_order_relaxed),
                     worker_id);
        return E_ARG_INVALID;
    }

    DC_COMMON_ASSERT(task->t_task_status == T_TASK_INIT);

    task->t_task_status = T_TASK_TO_RUN;
    task_list_[worker_id].push_back(task);

    LOG(DC_COMMON_LOG_INFO,
        "task:%s start in worker:%d",
        task->t_task_uuid.c_str(),
        worker_id);

    task_cond_list_[worker_id].notify_one();

    return S_SUCCESS;
}

dc_common_code_t
dc_compare_t::cancel(std::string task_uuid) {
    DC_COMMON_ASSERT(!task_uuid.empty());
    DC_COMMON_ASSERT(thd_list_.size() > 0);

    // hash task->t_task_uuid and get worker_id
    uint32_t worker_id = std::hash<std::string> {}(task_uuid) % thd_list_.size();

    std::lock_guard<std::mutex> lock(task_mutex_list_[worker_id]);

    // 看一下是不是已经存在了
    auto iter = task_status_list_[worker_id].find(task_uuid);
    if (iter == task_status_list_[worker_id].end()) {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task:%s not exist in worker:%d",
                     task_uuid.c_str(),
                     worker_id);
        return E_ARG_INVALID;
    }

    auto task = iter->second;
    DC_COMMON_ASSERT(task != nullptr);

    LOG(DC_COMMON_LOG_INFO,
        "task:%p task:%s cancel in worker:%d Q.size = %d",
        task,
        task->t_task_uuid.c_str(),
        worker_id,
        task_list_[worker_id].size());

    if (task->t_task_status == T_TASK_OVER) {
        // remove this task
        task_status_list_[worker_id].erase(iter);

        // set the file name to t_task_uuid;
        std::string file_name = task->t_task_uuid;
        // unlink the file = remove file!
        unlink(file_name.c_str());

        delete task;
    }

    return S_SUCCESS;
}

dc_common_code_t
dc_compare_t::result(std::string task_uuid,
                     char *task_out_buf /*nullable */,
                     const uint32_t task_out_buf_len /* can be 0*/,
                     int *result_bytes) {
    DC_COMMON_ASSERT(!task_uuid.empty());
    DC_COMMON_ASSERT(thd_list_.size() > 0);
    DC_COMMON_ASSERT(result_bytes != nullptr);

    // find task in task_status_list_
    // hash task->t_task_uuid and get worker_id
    uint32_t worker_id = std::hash<std::string> {}(task_uuid) % thd_list_.size();
    std::lock_guard<std::mutex> lock(task_mutex_list_[worker_id]);

    // 看一下是不是已经不存在了
    auto iter = task_status_list_[worker_id].find(task_uuid);
    if (iter == task_status_list_[worker_id].end()) {
        LOG_ROOT_ERR(E_ARG_INVALID,
                     "task:%s not exist in worker:%d",
                     task_uuid.c_str(),
                     worker_id);
        return E_ARG_INVALID;
    }

    auto task = iter->second;
    if (task->t_task_status == T_TASK_OVER) {
        LOG(DC_COMMON_LOG_INFO,
            "task:%s is over, begin to work on get result",
            task->t_task_uuid.c_str());

        auto &filename = task->t_task_uuid;

        // use system call fstat to get file length
        struct stat file_stat;
        if (stat(filename.c_str(), &file_stat) != 0) {
            LOG_ROOT_ERR(E_OS_ENV_STAT,
                         "task:%s result file:%s not exist",
                         task_uuid.c_str(),
                         filename.c_str());
            return E_OS_ENV_STAT;
        }

        *result_bytes = file_stat.st_size;

        // task over
        // copy result to task_out_buf
        if (task_out_buf != nullptr && task_out_buf_len > 0) {
            if (task_out_buf_len < file_stat.st_size) {
                LOG_ROOT_ERR(E_ARG_INVALID,
                             "task:%s result buf len:%d not enough:%lu",
                             task_uuid.c_str(),
                             task_out_buf_len,
                             (uint32_t)file_stat.st_size);
                return E_ARG_INVALID;
            }

            // get file content and copy to task_out_buf
            FILE *fp = fopen(filename.c_str(), "r");
            if (fp == nullptr) {
                LOG_ROOT_ERR(E_OS_ENV_OPEN,
                             "task:%s result file:%s open failed",
                             task_uuid.c_str(),
                             filename.c_str());
                return E_OS_ENV_OPEN;
            }

            // read all content into task_out_buf
            if (fread(task_out_buf, file_stat.st_size, 1, fp) != 1) {
                LOG_ROOT_ERR(E_OS_ENV_READ,
                             "task:%s result file:%s read failed",
                             task_uuid.c_str(),
                             filename.c_str());
                fclose(fp);
                return E_OS_ENV_READ;
            }

            fclose(fp);
            fp = nullptr;
        }
        return S_SUCCESS;
    }

    if (task->t_task_status == T_TASK_CANCEL) {
        // task cancel
        LOG(DC_COMMON_LOG_INFO,
            "task:%p task:%s cancel in worker:%d",
            task,
            task->t_task_uuid.c_str(),
            worker_id);
        return E_DC_TASK_HAS_BEEN_CANCELED;
    }

    // task not over
    return E_DC_TASK_MEM_VOPS_NOT_OVER;
}

static uint64_t
dc_compare_exe_time_to_usec(struct timeval t) {
    return (uint64_t)((uint64_t)t.tv_sec * 1000 * 1000 + t.tv_usec);
}

dc_common_code_t
dc_compare_t::check_task_is_failed(dc_api_task_t* &task) {
    DC_COMMON_ASSERT(task != nullptr);

    if (task->t_task_status == T_TASK_CANCEL) {
        LOG_ROOT_ERR(E_DC_TASK_HAS_BEEN_CANCELED,
                     "task:%s has been canceled",
                     task->t_task_uuid.c_str());
        return E_DC_TASK_HAS_BEEN_CANCELED;
    }

    // if all sql has been failed!
    if (task->t_sql_error_nr == task->t_server_info_arr.size()) {
        // set task error flag, and set it as over.
        // record job end time
        for (auto &server: task->t_server_info_arr) {
            gettimeofday(&server.c_end_time, NULL);
            server.c_total_time_ms = (dc_compare_exe_time_to_usec(server.c_end_time) -
                                      dc_compare_exe_time_to_usec(server.c_begin_time)) / 1000;
        }

        task->t_task_status = T_TASK_OVER;
        LOG_ROOT_ERR(E_DC_COMPARE_EXE_TASK_FAILED,
                     "t_sql_error_nr:%d == t_server_info_arr.size():%d",
                     task->t_sql_error_nr,
                     task->t_server_info_arr.size());

        return E_DC_COMPARE_EXE_TASK_FAILED;
    }

    // if std server has been failed
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());

    if (task->t_server_info_arr[task->t_std_idx].c_error != S_SUCCESS) {
        // set task error flag, and set it as over.
        // record job end time
        for (auto &server: task->t_server_info_arr) {
            gettimeofday(&server.c_end_time, NULL);
            server.c_total_time_ms = (dc_compare_exe_time_to_usec(server.c_end_time) -
                                      dc_compare_exe_time_to_usec(server.c_begin_time)) / 1000;
        }

        task->t_task_status = T_TASK_OVER;
        LOG_ROOT_ERR(E_DC_COMPARE_EXE_TASK_FAILED,
                     "std server has been failed, error:%d",
                     task->t_server_info_arr[0].c_error);

        return E_DC_COMPARE_EXE_TASK_FAILED;
    }

    if (task->t_sql_error_nr == task->t_server_info_arr.size() - 1 &&
            task->t_sql_error_nr > 0) {
        // set task error flag, and set it as over.
        // record job end time
        for (auto &server: task->t_server_info_arr) {
            gettimeofday(&server.c_end_time, NULL);
            server.c_total_time_ms = (dc_compare_exe_time_to_usec(server.c_end_time) -
                                      dc_compare_exe_time_to_usec(server.c_begin_time)) / 1000;
        }

        task->t_task_status = T_TASK_OVER;
        LOG_ROOT_ERR(E_DC_COMPARE_EXE_TASK_FAILED,
                     "t_sql_error_nr:%d == t_server_info_arr.size():%d - 1, there is just one server is ok!",
                     task->t_sql_error_nr,
                     task->t_server_info_arr.size());

        return E_DC_COMPARE_EXE_TASK_FAILED;
    }

    // 还可以继续跑!
    return S_SUCCESS;
}

/*
 * A为标准方!
 * A: 需要读取B, C上文件的某一行参与比较
 *
 * 第一步：A, B, C通读整个文件。然后需要记录每一行的以下信息：
 *
 *        <行号，此行在文件的pos, 行的长度，行内容生成的SHA1>
 *
 * 第二步：然后B, C将结果列表，压缩后发给A
 *
 * 第三步：A将收到的排序后的列表，过滤到相同的行。
 *
 * 第四步：将需要读取的行信息发送给B, C。B, C将相应行读出，压缩后发给A
 *
 * 第五步：由A生成差异!
 */
dc_common_code_t
dc_compare_t::exe_sql_job_for_file(dc_api_task_t *task, const int worker_id, const char *file_path)
{
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());

    return ret;
}

dc_common_code_t
dc_compare_t::exe_sql_job_for_dir(dc_api_task_t *task, const int worker_id, const char *dir_path)
{
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());

    // 递归遍历目录
    // 遍历目录下的所有文件
    // 对于每个文件，都要执行一次exe_sql_job_for_file
    // 对于每个文件，都要执行一次exe_sql_job_for_dir
    // linux scan dir
    DIR *d;
    struct dirent *dir;
    d = opendir(dir_path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_DIR) {
                if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                    continue;
                }
                std::string new_path = std::string(dir_path) + "/" + std::string(dir->d_name);
                ret = exe_sql_job_for_dir(task, worker_id, new_path.c_str());
                LOG_CHECK_ERR_RETURN(ret);
            } else {
                std::string new_path = std::string(dir_path) + "/" + std::string(dir->d_name);
                ret = exe_sql_job_for_file(task, worker_id, new_path.c_str());
                LOG_CHECK_ERR_RETURN(ret);
            }
        }
        closedir(d);
    }

    // TODO 比标准方多出来的文件如何处理？

    return S_SUCCESS;
}

// 注意，这里运行的都是block的请求
// 注意，这里并不去管理task的生命周期，只是去执行task
dc_common_code_t
dc_compare_t::exe_sql_job(dc_api_task_t *task, const int worker_id)
{
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());

    // 这里要做的事情：
    // 首先看要比较的是文件还是目录：
    // 1. 如果只是单个的文件，那么就直接比较
    //

    auto &std_server_info = task->t_server_info_arr[task->t_std_idx];
    auto &compare_file_path = std_server_info.c_path_to_compare;

    // check compare_file_path is DIR or file
    struct stat file_stat;
    if (stat(compare_file_path.c_str(), &file_stat) != 0) {
        LOG_ROOT_ERR(E_DC_COMPARE_EXE_TASK_FAILED,
                     "stat compare_file_path:%s failed, error:%s",
                     compare_file_path.c_str(),
                     strerror(errno));
        return E_DC_COMPARE_EXE_TASK_FAILED;
    }

    if (S_ISREG(file_stat.st_mode)) {
        // 如果是文件，那么就直接比较
        ret = exe_sql_job_for_file(task, worker_id, compare_file_path.c_str());
    } else if (S_ISDIR(file_stat.st_mode)) {
        // 如果是目录，那么就需要一个文件一个文件地比，并且生成相应的结果
        ret = exe_sql_job_for_dir(task, worker_id, compare_file_path.c_str());
    } else {
        LOG_ROOT_ERR(E_DC_COMPARE_EXE_TASK_FAILED,
                     "compare_file_path:%s is not a file or dir",
                     compare_file_path.c_str());
        return E_DC_COMPARE_EXE_TASK_FAILED;
    }

    // 2. 如果是目录，那么就需要一个文件一个文件地比，并且生成相应的结果
    //

    return ret;
}

dc_api_task_t*
dc_compare_t::pop_one_task(const int worker_id) {
    DC_COMMON_ASSERT(worker_id >= 0);
    DC_COMMON_ASSERT((int)worker_id < (int)task_list_.size());

    dc_api_task_t *task = nullptr;

    std::unique_lock<std::mutex> lock(task_mutex_list_[worker_id]);
    task_cond_list_[worker_id].wait_for(lock,
                                        std::chrono::milliseconds(100),
                                        [this, worker_id] { return !task_list_[worker_id].empty(); });

    if (!task_list_[worker_id].empty()) {
        task = task_list_[worker_id].front();
        task_list_[worker_id].pop_front();
    }

    // if task not null, then MUST exist in status_list
    if (task != nullptr) {
        auto iter = task_status_list_[worker_id].find(task->t_task_uuid);
        DC_COMMON_ASSERT(iter != task_status_list_[worker_id].end());
    }

    // 如果这个任务已经是cancel状态了，那么就不需要再执行了
    if (task != nullptr && task->t_task_status == T_TASK_CANCEL) {
        LOG(DC_COMMON_LOG_INFO,
            "task:%s is cancel, remove from hash_worker",
            task->t_task_uuid.c_str());
        task_status_list_[worker_id].erase(task->t_task_uuid);
        delete task;
        task = nullptr;
    }

    return task;
}

dc_common_code_t
dc_compare_t::try_free_db_list(int worker_id) {
    dc_common_code_t ret = S_SUCCESS;

    std::vector<std::list<dc_content_t*>::iterator> to_delete_list;

    // iterate the task_db_list_ of worker_id
    auto &db_list = task_db_list_[worker_id];
    for (auto iter = db_list.begin(); iter != db_list.end(); iter++) {
        auto &db = *iter;
        // TODO
        // ret = db->end_recv_sql_result();
        if (ret == S_SUCCESS) {
            delete db;
            to_delete_list.push_back(iter);
        } else if (ret == E_DC_DB_RETRY) {
            // nothing to do
        } else {
            // error happened
            DC_COMMON_ASSERT(0);
        }
    }

    for (auto &iter: to_delete_list) {
        db_list.erase(iter);
    }

    return ret;
}

dc_common_code_t dc_compare_t::execute(const int worker_id) {
    dc_common_code_t ret;

    while (!exit_) {
        if (exit_) {
            break;
        }

        auto task = pop_one_task(worker_id);
        if (task == nullptr) {
            continue;
        }

        ret = exe_sql_job(task, worker_id);
        LOG_CHECK_ERR(ret);
        if (ret != S_SUCCESS) {
            dc_diff_failed_content_t diff { task };
            diff.gen_diff(task);
        }

        try_free_db_list(worker_id);

    } // end while

    LOG(DC_COMMON_LOG_INFO, "main worker:%d is over", worker_id);

    while (task_db_list_[worker_id].size() > 0) {
        try_free_db_list(worker_id);
    }

    return S_SUCCESS;
}
