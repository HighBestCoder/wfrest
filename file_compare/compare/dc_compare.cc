#include "dc_compare.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <vector>

#include "dc_common_assert.h"
#include "dc_common_error.h"
#include "dc_common_trace_log.h"
#include "dc_diff.h"
#include "dc_diff_content.h"
#include "dc_diff_failed_content.h"

std::pair<std::string, std::string> get_base_and_file_name(std::string compare_file_path) {
    size_t pos = compare_file_path.find_last_of('/');

    std::string dir_name = (pos != std::string::npos) ? compare_file_path.substr(0, pos) : ".";

    std::string file_name = (pos != std::string::npos) ? compare_file_path.substr(pos + 1) : compare_file_path;

    return std::make_pair(dir_name, file_name);
}

dc_compare_t::dc_compare_t(void) {
    exit_ = false;
    worker_threads_ = new std::thread(&dc_compare_t::execute, this);
    DC_COMMON_ASSERT(worker_threads_ != nullptr);
}

dc_compare_t::~dc_compare_t() {
    exit_ = true;
    worker_threads_->join();
}

// 如果添加成功，那么接过生命周期
dc_common_code_t dc_compare_t::add(dc_api_task_t *task) {
    DC_COMMON_ASSERT(task != nullptr);
    task_q_.write(task);
    return S_SUCCESS;
}

dc_common_code_t dc_compare_t::get(wfrest::Json &result_json) {
    dc_api_task_t *task = nullptr;
    bool has_task = out_q_.read_once((void **)&task);
    if (!has_task) {
        return E_DC_TASK_MEM_VOPS_NOT_OVER;
    }

    DC_COMMON_ASSERT(task != nullptr);
    result_json.swap(task->t_compare_result_json);
    delete task;
    return S_SUCCESS;
}

static bool assign_attr_to_json(wfrest::Json &json, const std::string &center_name, const dc_file_attr_t &attr) {
    json["server_name"] = center_name;
    json["size"] = attr.f_size;
    json["owner"] = attr.f_owner;
    json["last_updated"] = attr.f_last_updated;
    json["mode"] = attr.f_mode;

    return true;
}

static bool assign_attr_to_json(wfrest::Json &json, const std::string &center_name, const dc_file_attr_t &attr,
                                const std::string &md5) {
    json["server_name"] = center_name;
    json["size"] = attr.f_size;
    json["owner"] = attr.f_owner;
    json["last_updated"] = attr.f_last_updated;
    json["mode"] = attr.f_mode;
    json["md5"] = md5;

    return true;
}

// 返回值true表示往json里面添加了内容
// 返回值false表示没有往json里面添加内容
static bool compare_assign_attr_to_json(const dc_file_attr_t &std, const dc_file_attr_t &other, wfrest::Json &json,
                                        const std::string &center_name) {
    if (other.f_code != S_SUCCESS) {
        json["errno"] = other.f_code;
        json["error"] = dc_common_code_msg(other.f_code);
        return true;
    }

    if (std.f_code != S_SUCCESS) {
        return assign_attr_to_json(json, center_name, other);
    }

    DC_COMMON_ASSERT(std.f_code == S_SUCCESS);
    DC_COMMON_ASSERT(other.f_code == S_SUCCESS);

    // 如果完全一样!
    if (std.compare(other)) {
        return false;
    }

    // 如果不一样!
    json["server_name"] = center_name;
    // if f_size is not the same
    if (std.f_size != other.f_size) {
        json["size"] = other.f_size;
    }
    // if f_owner is not the same
    if (std.f_owner != other.f_owner) {
        json["owner"] = other.f_owner;
    }
    // if f_last_updated is not the same
    if (std.f_last_updated != other.f_last_updated) {
        json["last_updated"] = other.f_last_updated;
    }
    // if f_mode is not the same
    if (std.f_mode != other.f_mode) {
        json["mode"] = other.f_mode;
    }

    return true;
}

// 返回值true表示往json里面添加了内容
// 返回值false表示没有往json里面添加内容
static bool compare_assign_attr_to_json(const dc_file_attr_t &std, dc_file_attr_t &other, wfrest::Json &json,
                                        const std::string &center_name, const std::string &std_md5,
                                        const std::string &other_md5) {
    if (other.f_code != S_SUCCESS) {
        json["errno"] = other.f_code;
        json["error"] = dc_common_code_msg(other.f_code);
        return true;
    }

    if (std.f_code != S_SUCCESS) {
        return assign_attr_to_json(json, center_name, other, other_md5);
    }

    DC_COMMON_ASSERT(std.f_code == S_SUCCESS);
    DC_COMMON_ASSERT(other.f_code == S_SUCCESS);

    // 如果完全一样!
    if (std.compare(other)) {
        return false;
    }

    // 如果不一样!
    json["server_name"] = center_name;
    // if f_size is not the same
    if (std.f_size != other.f_size) {
        json["size"] = other.f_size;
    }
    // if f_owner is not the same
    if (std.f_owner != other.f_owner) {
        json["owner"] = other.f_owner;
    }
    // if f_last_updated is not the same
    if (std.f_last_updated != other.f_last_updated) {
        json["last_updated"] = other.f_last_updated;
    }
    // if f_mode is not the same
    if (std.f_mode != other.f_mode) {
        json["mode"] = other.f_mode;
    }
    // if f_md5 is not the same
    if (std_md5 != other_md5) {
        json["md5"] = other_md5;
    }

    return true;
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
 * 第三步：A将收到列表，过滤到相同的行。
 *
 * 第四步：将需要读取的行信息发送给B, C。B, C将相应行读出，压缩后发给A
 *
 * 第五步：由A生成差异!
 */
dc_common_code_t dc_compare_t::exe_sql_job_for_single_item_file(dc_api_task_t *task, const char *file_rull_path,
                                                                const int json_idx,
                                                                std::vector<dc_content_t *> &content_list) {
    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());
    DC_COMMON_ASSERT(file_rull_path != nullptr);
    DC_COMMON_ASSERT(task->t_server_info_arr.size() == content_list.size());
    // 一次发一个文件!
    return S_SUCCESS;
}

dc_common_code_t dc_compare_t::exe_sql_job_for_single_item_dir1(dc_api_task_t *task,
                                                                std::vector<dc_content_t *> &content_list) {
    if (dir_q_.empty()) {
        return S_SUCCESS;
    }

    // 就一次性把目录都发完
    std::vector<std::string> dir_list;
    for (auto &dir : dir_q_) {
        dir_list.push_back(dir.first);
    }

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());
    DC_COMMON_ASSERT(task->t_server_info_arr.size() == content_list.size());

    std::vector<std::vector<dc_file_attr_t>> dir_attr_list(content_list.size());
    std::vector<bool> has_done(task->t_server_info_arr.size(), false);
    std::vector<int> has_failed(task->t_server_info_arr.size(), false);
    int has_done_cnt = 0;

    for (auto i = 0; i < content_list.size(); i++) {
        auto &content = content_list[i];
        auto ret = content->do_dir_list_attr(&dir_list, &dir_attr_list[i]);
        LOG_CHECK_ERR(ret);
        if (ret != S_SUCCESS) {
            has_done[i] = true;
            has_done_cnt++;
            has_failed[i] = ret;
        }
    }

    while (has_done_cnt < content_list.size()) {
        for (auto i = 0; i < content_list.size(); i++) {
            if (has_done[i]) {
                continue;
            }

            auto &content = content_list[i];
            auto ret = content->get_dir_list_attr();
            if (ret == E_DC_CONTENT_RETRY) {
                continue;
            }

            has_done[i] = true;
            has_done_cnt++;
            LOG_CHECK_ERR(ret);

            if (ret != S_SUCCESS) {
                has_failed[i] = ret;
            }
        }
    }

    // 这里应该是拿到所有的目录的属性了
    // 我们遍历dir_q_中的所有目录
    for (auto dir_idx = 0; dir_idx < dir_q_.size(); dir_idx++) {
        const auto &dir_path = dir_q_[dir_idx].first;
        const auto &dir_json_idx = dir_q_[dir_idx].second;
        DC_COMMON_ASSERT(dir_json_idx >= 0);
        DC_COMMON_ASSERT(dir_json_idx < (int)task->t_compare_result_json["diffs"].size());
        auto &ret_json = task->t_compare_result_json["diffs"][dir_json_idx];

        wfrest::Json server_json_list = nlohmann::json::array();

        DC_COMMON_ASSERT(task->t_std_idx >= 0);
        DC_COMMON_ASSERT(task->t_std_idx < (int)dir_attr_list.size());
        DC_COMMON_ASSERT(dir_attr_list.size() == has_failed.size());

        // 首先我们要为这个json生成std的结果!
        wfrest::Json std_cur_dir_json;
        std_cur_dir_json["is_standard"] = true;

        dc_file_attr_t std_cur_dir_attr;

        auto &std_dir_attr_list = dir_attr_list[task->t_std_idx];
        if (dir_idx >= std_dir_attr_list.size()) {
            std_cur_dir_json["errno"] = has_failed[task->t_std_idx];
            std_cur_dir_json["error"] = dc_common_code_msg((dc_common_code_t)has_failed[task->t_std_idx]);
            std_cur_dir_attr.f_code = (dc_common_code_t)has_failed[task->t_std_idx];
        } else {
            assign_attr_to_json(std_cur_dir_json, task->t_server_info_arr[task->t_std_idx].c_center,
                                std_dir_attr_list[dir_idx]);
            std_cur_dir_attr = std_dir_attr_list[dir_idx];
        }
        server_json_list.push_back(std_cur_dir_json);

        for (auto server_idx = 0; server_idx < content_list.size(); server_idx++) {
            if (server_idx == task->t_std_idx) {
                continue;
            }

            // 尝试取出这个server在dir_idx的结果
            auto &server_dir_attr = dir_attr_list[server_idx];
            wfrest::Json other_cur_dir_json;
            if (dir_idx >= server_dir_attr.size()) {
                // 那么json在这里需要说明这个server在这里出错了!
                other_cur_dir_json["errno"] = has_failed[server_idx];
                other_cur_dir_json["error"] = dc_common_code_msg((dc_common_code_t)has_failed[server_idx]);
                server_json_list.push_back(other_cur_dir_json);
            } else {
                auto &server_dir_attr_item = server_dir_attr[dir_idx];
                if (compare_assign_attr_to_json(std_cur_dir_attr, server_dir_attr_item, other_cur_dir_json,
                                                task->t_server_info_arr[server_idx].c_center)) {
                    server_json_list.push_back(other_cur_dir_json);
                }
            }
        }

        ret_json["servers"] = server_json_list;
    }

    dir_q_.clear();

    return S_SUCCESS;
}

dc_common_code_t dc_compare_t::exe_sql_job_for_single_item_dir(dc_api_task_t *task, const char *dir_full_path,
                                                               const int json_idx,
                                                               std::vector<dc_content_t *> &content_list) {
    dc_common_code_t ret = S_SUCCESS;
    const int dir_batch_size = 1000;

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());
    DC_COMMON_ASSERT(dir_full_path != nullptr);
    DC_COMMON_ASSERT(task->t_server_info_arr.size() == content_list.size());

    // 一次发很多目录!
    dir_q_.push_back({std::string(dir_full_path), json_idx});

    if (dir_q_.size() > dir_batch_size) {
        ret = exe_sql_job_for_single_item_dir1(task, content_list);
        LOG_CHECK_ERR_RETURN(ret);
    }

    return ret;
}

dc_common_code_t dc_compare_t::exe_sql_job_for_dir(dc_api_task_t *task, const char *path, int depth,
                                                   std::vector<dc_content_t *> &content_list) {
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());
    DC_COMMON_ASSERT(task->t_server_info_arr.size() == content_list.size());

    DIR *d = nullptr;
    struct dirent *dir = nullptr;
    auto &json_array = task->t_compare_result_json["diffs"];

    auto get_parent_dir = [&depth, &path](void) -> std::string {
        if (depth == 0) {
            return ".";
        }

        std::string parent_dir = path;
        if (parent_dir.back() == '/') {
            parent_dir.pop_back();
        }

        auto pos = parent_dir.find_last_of('/');
        if (pos == std::string::npos) {
            return parent_dir;
        }

        return parent_dir.substr(pos);
    };

    d = opendir(path);
    if (!d) {
        LOG_ROOT_ERR(E_OS_ENV_OPEN, "task:%s open dir:%s failed", task->t_task_uuid.c_str(), path);
        return E_OS_ENV_OPEN;
    }

    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_LNK) {
            // 我们只比较目录和文件！
            continue;
        }

        std::string new_path = std::string(path) + "/" + std::string(dir->d_name);

        if (dir->d_type == DT_DIR) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            json_array.emplace_back();
            json_array.back()["name"] = dir->d_name;
            json_array.back()["dir"] = get_parent_dir();
            json_array.back()["is_dir"] = true;

            ret = exe_sql_job_for_single_item_dir(task, new_path.c_str(), json_array.size() - 1, content_list);
            LOG_CHECK_ERR(ret);

            ret = exe_sql_job_for_dir(task, new_path.c_str(), depth + 1, content_list);
            LOG_CHECK_ERR(ret);
        } else {
            struct stat st;
            if (stat(new_path.c_str(), &st) == -1) {
                LOG_ROOT_ERR(E_OS_ENV_STAT, "stat %s failed", new_path.c_str());
                return E_OS_ENV_STAT;
            }

            if (!S_ISREG(st.st_mode)) {
                continue;
            }

            json_array.emplace_back();
            json_array.back()["name"] = dir->d_name;
            json_array.back()["dir"] = get_parent_dir();
            json_array.back()["is_dir"] = false;
            json_array.back()["path"] = new_path;

            ret = exe_sql_job_for_single_item_file(task, new_path.c_str(), json_array.size() - 1, content_list);
            LOG_CHECK_ERR_RETURN(ret);
        }
    }

    closedir(d);

    return S_SUCCESS;
}

// 注意，这里并不去管理task的生命周期，只是去执行task
dc_common_code_t dc_compare_t::exe_sql_job1(dc_api_task_t *task, std::vector<dc_content_t *> &content_list) {
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());

    auto &std_server_info = task->t_server_info_arr[task->t_std_idx];
    auto &compare_file_path = std_server_info.c_path_to_compare;
    auto &json_array = task->t_compare_result_json["diffs"];

    struct stat file_stat;

    // 如果标装方stat出错!
    if (stat(compare_file_path.c_str(), &file_stat) != 0) {
        // 标准方打开失败，那就不要再继续比较了
        LOG_ROOT_ERR(E_OS_ENV_STAT, "task:%s stat %s failed", task->t_task_uuid.c_str(), compare_file_path.c_str());
        return E_OS_ENV_STAT;
    }

    if (S_ISREG(file_stat.st_mode)) {
        // 这里只需要比较单个的文件!
        json_array.emplace_back();
        ret = exe_sql_job_for_single_item_file(task, compare_file_path.c_str(), json_array.back(), content_list);
        LOG_CHECK_ERR_RETURN(ret);

        return ret;
    }

    if (S_ISDIR(file_stat.st_mode)) {
        // 如果是目录，那么就需要一个文件一个文件地比，并且生成相应的结果
        // std server 使用chdir跳到自己的basedir
        int err = chdir(task->t_server_info_arr[task->t_std_idx].c_base_dir.c_str());
        if (err) {
            LOG_ROOT_ERR(E_OS_ENV_CHDIR, "task:%s chdir:%s failed errno:%d error:%s", task->t_task_uuid.c_str(),
                         task->t_server_info_arr[task->t_std_idx].c_base_dir.c_str(), errno, strerror(errno));
        }

        // server_info.base_dir + compare_file_path才是完整的绝对路径！
        // 如果就是base_dir开始比较，那么compare_file_path = '.';
        ret = exe_sql_job_for_dir(task, compare_file_path.c_str(), 0, content_list);
        LOG_CHECK_ERR_RETURN(ret);

        if (dir_q_.size() > 0) {
            // TODO 把这里所有的目录一次性全部发掉
        }

        return ret;
    }

    LOG_ROOT_ERR(E_NOT_SUPPORT_FILE_TYPE, "task:%s %s is not a file or dir, not support file type",
                 task->t_task_uuid.c_str(), compare_file_path.c_str());
    return E_NOT_SUPPORT_FILE_TYPE;
}

dc_common_code_t dc_compare_t::exe_sql_job(dc_api_task_t *task) {
    DC_COMMON_ASSERT(task != nullptr);

    // generate dc_content list
    dc_common_code_t ret = S_SUCCESS;
    std::vector<dc_content_t *> dc_content_list;
    for (int i = 0; i < (int)task->t_server_info_arr.size(); i++) {
        auto &server_info = task->t_server_info_arr[i];
        if (i == task->t_std_idx) {
            auto local_content_reader = new dc_content_local_t(&server_info);
            dc_content_list.emplace_back(local_content_reader);
        } else {
            // TODO convert to remote_content
            auto remote_content_reader = new dc_content_local_t(&server_info);
            dc_content_list.emplace_back(remote_content_reader);
        }
    }

    ret = exe_sql_job1(task, dc_content_list);
    LOG_CHECK_ERR(ret);

    for (auto &d : dc_content_list) {
        DC_COMMON_ASSERT(d);
        delete d;
    }

    return ret;
}

dc_common_code_t dc_compare_t::execute() {
    dc_common_code_t ret;

    while (!exit_) {
        if (exit_) {
            break;
        }

        dc_api_task_t *task = nullptr;
        bool has_task = task_q_.read_once((void **)&task);
        if (!has_task) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        DC_COMMON_ASSERT(task != nullptr);

        // 先做好一个空的json
        // exe_sql_job在执行的时候，只是往这个diffs里面填充内容!
        task->t_compare_result_json["uuid"] = task->t_task_uuid;
        task->t_compare_result_json["diffs"] = wfrest::Json::array();
        task->t_compare_result_json["next_shard"] = -1;

        ret = exe_sql_job(task);
        LOG_CHECK_ERR(ret);

        if (ret != S_SUCCESS) {
            task->t_compare_result_json["errno"] = ret;
            task->t_compare_result_json["error"] = dc_common_code_msg(ret);
        }

        out_q_.write((void *)task);
    }  // end while

    return S_SUCCESS;
}
