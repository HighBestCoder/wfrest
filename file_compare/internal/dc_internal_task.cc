#include "dc_internal_task.h"

#include <dirent.h>
#include <pwd.h>
#include <string.h>
#include <sys/types.h>

#include "dc_common_assert.h"
#include "dc_common_error.h"
#include "dc_common_trace_log.h"

dc_internal_executor_t::dc_internal_executor_t() {
    thd_worker_ = new std::thread(&dc_internal_executor_t::thd_worker_execute, this);
    DC_COMMON_ASSERT(thd_worker_ != nullptr);
}

dc_internal_executor_t::~dc_internal_executor_t() {
    exit_ = true;
    if (thd_worker_ != nullptr) {
        thd_worker_->join();
        delete thd_worker_;
        thd_worker_ = nullptr;
    }
}

dc_common_code_t dc_internal_executor_t::add(dc_internal_task_t task) {
    std::lock_guard<std::mutex> lock(mutex_);
    task_list_.push_back(task);
    cond_.notify_one();
    return S_SUCCESS;
}

dc_common_code_t dc_internal_executor_t::get(const std::string &uuid, wfrest::Json &result_json /*OUT*/) {
    std::lock_guard<std::mutex> lock(mutex_);

    // use iterator to find the task
    for (auto it = task_list_.begin(); it != task_list_.end(); ++it) {
        if (it->i_uuid == uuid) {
            if (it->i_task_status == T_TASK_OVER) {
                result_json.swap(it->i_out_json);
                task_list_.erase(it);
                return S_SUCCESS;
            } else {
                return E_DC_CONTENT_RETRY;
            }
        }
    }

    return E_NOT_FOUND;
}

bool dc_internal_executor_t::thd_worker_has_task_to_run() {
    for (auto &task : task_list_) {
        if (task.i_task_status == T_TASK_INIT) {
            return true;
        }
    }
    return false;
}

dc_internal_task_t &dc_internal_executor_t::thd_worker_pick_task_to_run() {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!exit_ && !thd_worker_has_task_to_run()) {
        cond_.wait(lock);
    }

    for (auto &task : task_list_) {
        if (task.i_task_status == T_TASK_INIT) {
            task.i_task_status = T_TASK_RUNNING;
            return task;
        }
    }

    DC_COMMON_ASSERT(0 == "should not hit here");
}

void dc_internal_executor_t::thd_worker_set_task_over(dc_internal_task_t &task) {
    std::lock_guard<std::mutex> lock(mutex_);
    task.i_task_status = T_TASK_OVER;
}

void dc_internal_executor_t::thd_worker_task_dir_attr_list(dc_internal_task_t &task) {
    DC_COMMON_ASSERT(task.i_task_type == DC_INTERNAL_TASK_TYPE_DIR_ATTR);
    DC_COMMON_ASSERT(task.i_task_status == T_TASK_RUNNING);
    DC_COMMON_ASSERT(task.i_json.find("uuid") != task.i_json.end());
    DC_COMMON_ASSERT(task.i_json.find("dirs") != task.i_json.end());
    DC_COMMON_ASSERT(task.i_json.find("base_dir") != task.i_json.end());

    // we need to chdir to the base dir.
    std::string base_dir = task.i_json["base_dir"];

    int err = chdir(base_dir.c_str());
    if (err != 0) {
        LOG_ROOT_ERR(E_OS_ENV_CHDIR, "task:%s chdir to base_dir:%s failed, errno:%d, error:%s", task.i_uuid.c_str(),
                     base_dir.c_str(), errno, strerror(errno));
        task.i_out_json["error"] = E_OS_ENV_CHDIR;
        task.i_out_json["uuid"] = task.i_json["uuid"];
        thd_worker_set_task_over(task);
        return;
    }

    std::vector<dc_file_attr_t> dir_attr_list;
    for (auto &dir : task.i_json["dirs"]) {
        dir_attr_list.emplace_back();
        dc_file_attr_t &dir_attr = dir_attr_list.back();

        // 由于是目录，那么我们在取attr的时候，只需要取目录需要的就可以了.
        struct stat dir_stat;
        std::string dir_str = dir;
        err = stat(dir_str.c_str(), &dir_stat);
        if (err != 0) {
            LOG_ROOT_ERR(E_OS_ENV_STAT, "task:%s lstat dir:%s failed, errno:%d, error:%s", task.i_uuid.c_str(),
                         dir_str.c_str(), errno, strerror(errno));
            dir_attr.f_code = E_OS_ENV_STAT;
            continue;
        }

        dir_attr.f_code = S_SUCCESS;

        // if the file is a directory
        // return error, here just support file
        DC_COMMON_ASSERT(S_ISDIR(dir_stat.st_mode));

        // get file mode, then convert st_mode to string
        char mode_str[128] = {0};
        snprintf(mode_str, sizeof(mode_str), "%o", dir_stat.st_mode & 0777);
        dir_attr.f_mode = mode_str;

        // get owner name by st_uid
        struct passwd *pwd = getpwuid(dir_stat.st_uid);
        if (pwd == nullptr) {
            if (errno == 0) {
                dir_attr.f_owner = "0";
            } else {
                LOG_ROOT_ERR(E_OS_ENV_GETPWUID, "task:%s path:%s getpwuid failed, errno=%d, errstr=%s",
                             task.i_uuid.c_str(), dir_str.c_str(), errno, strerror(errno));
                dir_attr.f_code = E_OS_ENV_GETPWUID;
                continue;
            }
        } else {
            dir_attr.f_owner = pwd->pw_name;
        }

        // get file last updated time, convert st_mtime to string
        char time_str[32] = {0};
        struct tm *tm = localtime(&dir_stat.st_mtime);
        if (tm == nullptr) {
            LOG_ROOT_ERR(E_OS_ENV_LOCALTIME, "localtime failed, errno=%d, errstr=%s", errno, strerror(errno));
            dir_attr.f_code = E_OS_ENV_LOCALTIME;
            continue;
        }

        // format tm to string
        strftime(time_str, sizeof(time_str), "%Y%m%d%-H%M%S", tm);
        dir_attr.f_last_updated = time_str;
    }

    // convert dir_attr_list to json
    task.i_out_json["uuid"] = task.i_json["uuid"];
    task.i_out_json["error"] = S_SUCCESS;
    for (auto &dir_attr : dir_attr_list) {
        wfrest::Json dir_attr_json;
        dir_attr_json["error"] = dir_attr.f_code;
        dir_attr_json["mode"] = dir_attr.f_mode;
        dir_attr_json["owner"] = dir_attr.f_owner;
        dir_attr_json["last_updated"] = dir_attr.f_last_updated;
        task.i_out_json["dirs"].push_back(dir_attr_json);
    }

    thd_worker_set_task_over(task);
}

void dc_internal_executor_t::thd_worker_execute() {
    while (!exit_) {
        dc_internal_task_t &task = thd_worker_pick_task_to_run();

        if (task.i_task_type == DC_INTERNAL_TASK_TYPE_DIR_ATTR) {
            thd_worker_task_dir_attr_list(task);
        }
    }
}