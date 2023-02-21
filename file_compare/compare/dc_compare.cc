#include "dc_compare.h"
#include "dc_common_assert.h"
#include "dc_common_error.h"
#include "dc_diff.h"
#include "dc_diff_content.h"
#include "dc_diff_failed_content.h"
#include "dc_common_trace_log.h"

#include <stdlib.h>
#include <string.h>
#include <vector>
#include <functional>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

std::pair<std::string, std::string>
get_base_and_file_name(std::string compare_file_path)
{
    size_t pos = compare_file_path.find_last_of('/');

    std::string dir_name = (pos != std::string::npos) ?
                           compare_file_path.substr(0, pos) : ".";

    std::string file_name = (pos != std::string::npos) ?
                            compare_file_path.substr(pos + 1): compare_file_path;

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

dc_common_code_t
dc_compare_t::get(wfrest::Json &result_json)
{
    dc_api_task_t *task = nullptr;
    bool has_task = out_q_.read_once((void**)&task);
    if (!has_task) {
        return E_DC_TASK_MEM_VOPS_NOT_OVER;
    }

    DC_COMMON_ASSERT(task != nullptr);
    result_json.swap(task->t_compare_result_json);
    delete task;
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
 * 第三步：A将收到列表，过滤到相同的行。
 *
 * 第四步：将需要读取的行信息发送给B, C。B, C将相应行读出，压缩后发给A
 *
 * 第五步：由A生成差异!
 */
dc_common_code_t
dc_compare_t::exe_sql_job_for_file(dc_api_task_t *task,
                                   const char *c_file_path,
                                   bool is_dir,
                                   wfrest::Json &single_file_compare_result_json/*CHANGED*/)
{
    dc_common_code_t ret = S_SUCCESS;

    LOG(DC_COMMON_LOG_INFO, "[main] task:%s compare file:%s", task->t_task_uuid.c_str(), c_file_path);

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());

    const int task_number = task->t_server_info_arr.size();
    const int std_idx = task->t_std_idx;

    DC_COMMON_ASSERT(task_number > 0);
    DC_COMMON_ASSERT(std_idx >= 0);
    DC_COMMON_ASSERT(std_idx < task_number);

    std::vector<dc_content_t *> content_list;

    for (int i = 0; i < task_number; i++) {
        // TODO std 用local_content_t
        // other 用户remote_content_t
        dc_content_t *content = new dc_content_local_t(&task->t_server_info_arr[i]);
        DC_COMMON_ASSERT(content != nullptr);

        content_list.push_back(content);
    }

    std::string file_path(c_file_path);
    std::vector<dc_file_attr_t> file_attr_list(task_number);

    for (size_t i = 0; i < content_list.size(); i++) {
        ret = content_list[i]->do_file_attr(file_path, &file_attr_list[i]);
        LOG_CHECK_ERR_RETURN(ret);
    }

    std::vector<bool> job_has_done(task_number, false);

    int job_done_nr = 0;
    while (job_done_nr < task_number) {
        for (int i = 0; i < task_number; i++) {
            auto &content = content_list[i];
            if (job_has_done[i]) {
                continue;
            }

            dc_common_code_t ret = content->get_file_attr();
            if (ret == E_DC_CONTENT_RETRY) {
                continue;
            }

            if (ret == S_SUCCESS) {
                job_has_done[i] = true;
                job_done_nr++;
            } else {
                job_has_done[i] = true;
                LOG_CHECK_ERR(ret);
                job_done_nr++;
            }
        }
    }

    auto file_name_pair = get_base_and_file_name(file_path);
    single_file_compare_result_json["dir"] = file_name_pair.first;
    single_file_compare_result_json["name"] = file_name_pair.second;
    single_file_compare_result_json["servers"] = wfrest::Json::array();

    // set job_has_done to false
    for (int i = 0; i < task_number; i++) {
        job_has_done[i] = false;
    }
    std::vector<std::string> file_md5_list(task_number);

    if (!is_dir) {
        // get file content
        std::vector<std::vector<std::string>> file_content_list(task_number);
        std::vector<int> empty_lines_nr_list(task_number, 0);
        for (int i = 0; i < task_number; i++) {
            auto &content = content_list[i];
            ret = content->do_file_content(file_path,
                                           &file_content_list[i],
                                           &empty_lines_nr_list[i]);
            LOG_CHECK_ERR_RETURN(ret);
        }
    
        job_done_nr = 0;
        while (job_done_nr < task_number) {
            for (int i = 0; i < task_number; i++) {
                auto &content = content_list[i];
                if (job_has_done[i]) {
                    continue;
                }
    
                dc_common_code_t ret = content->get_file_content();
                if (ret == E_DC_CONTENT_RETRY) {
                    continue;
                }
    
                if (ret == S_SUCCESS) {
                    job_has_done[i] = true;
                    ret = content->get_file_md5(file_md5_list[i]);
                    LOG_CHECK_ERR_RETURN(ret);
                    job_done_nr++;
                } else {
                    job_has_done[i] = true;
                    LOG_CHECK_ERR(ret);
                    job_done_nr++;
                }
            }
        }
    }

    // compare the sha1 list
    for (int i = 0; i < task_number; i++) {
        if (i == std_idx) {
            wfrest::Json std_server_file_info;
            std_server_file_info["server_name"] = task->t_server_info_arr[i].c_center;
            std_server_file_info["size"] = file_attr_list[i].f_size;
            std_server_file_info["owner"] = file_attr_list[i].f_owner;
            std_server_file_info["last_updated"] = file_attr_list[i].f_last_updated;
            std_server_file_info["mode"] = file_attr_list[i].f_mode;
            std_server_file_info["is_standard"] = true;
            std_server_file_info["md5"] = file_md5_list[i];

            single_file_compare_result_json["servers"].push_back(std_server_file_info);
        } else {
            // compare the size, owner, last_updated, mode, md5
            auto is_same = file_attr_list[std_idx].compare(file_attr_list[i]);
            if (!is_same) {
                wfrest::Json diff_server_info;
                diff_server_info["server_name"] = task->t_server_info_arr[i].c_center;
                // if f_size is not the same
                if (file_attr_list[std_idx].f_size != file_attr_list[i].f_size) {
                    diff_server_info["size"] = file_attr_list[i].f_size;
                }
                // if f_owner is not the same
                if (file_attr_list[std_idx].f_owner != file_attr_list[i].f_owner) {
                    diff_server_info["owner"] = file_attr_list[i].f_owner;
                }
                // if f_last_updated is not the same
                if (file_attr_list[std_idx].f_last_updated != file_attr_list[i].f_last_updated) {
                    diff_server_info["last_updated"] = file_attr_list[i].f_last_updated;
                }
                // if f_mode is not the same
                if (file_attr_list[std_idx].f_mode != file_attr_list[i].f_mode) {
                    diff_server_info["mode"] = file_attr_list[i].f_mode;
                }
                // if f_md5 is not the same
                if (file_md5_list[std_idx] != file_md5_list[i]) {
                    diff_server_info["md5"] = file_md5_list[i];
                }
                single_file_compare_result_json["servers"].push_back(diff_server_info);
            }
        }
    }

    for (auto &content: content_list) {
        delete content;
    }

    content_list.clear();

    return ret;
}

dc_common_code_t
dc_compare_t::exe_sql_job_for_dir(dc_api_task_t *task, const char *dir_path)
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

    task->t_compare_result_json["diffs"].emplace_back();
    wfrest::Json &dir_json = task->t_compare_result_json["diffs"].back();
    dir_json["name"] = dir_path;
    ret = exe_sql_job_for_file(task, dir_path, true, dir_json);
    LOG_CHECK_ERR_RETURN(ret);

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
                ret = exe_sql_job_for_dir(task, new_path.c_str());
                LOG_CHECK_ERR_RETURN(ret);
            } else {
                std::string new_path = std::string(dir_path) + "/" + std::string(dir->d_name);

                task->t_compare_result_json["diffs"].emplace_back();
                wfrest::Json &single_file_compare_result_json = task->t_compare_result_json["diffs"].back();
                single_file_compare_result_json["name"] = dir->d_name;
                single_file_compare_result_json["dir"] = dir_path;

                // use stat to check new_path is a file?
                struct stat st;
                if(stat(new_path.c_str(), &st) == -1) {
                    LOG_ROOT_ERR(E_OS_ENV_STAT, "stat %s failed", new_path.c_str());
                    return E_OS_ENV_STAT;
                }

                if (!S_ISREG(st.st_mode)) {
                    continue;
                }

                ret = exe_sql_job_for_file(task,
                                           new_path.c_str(),
                                           false/*not_dir*/,
                                           single_file_compare_result_json);
                LOG_CHECK_ERR_RETURN(ret);
            }
        }
        closedir(d);
    }

    return S_SUCCESS;
}

// 注意，这里运行的都是block的请求
// 注意，这里并不去管理task的生命周期，只是去执行task
dc_common_code_t
dc_compare_t::exe_sql_job(dc_api_task_t *task)
{
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(task->t_std_idx >= 0);
    DC_COMMON_ASSERT(task->t_std_idx < (int)task->t_server_info_arr.size());

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

    LOG(DC_COMMON_LOG_INFO, "[main] begin to execute task:%s", task->t_task_uuid.c_str());

    task->t_compare_result_json["id"] = task->t_task_uuid;
    task->t_compare_result_json["next_shard"] = -1;
    task->t_compare_result_json["diffs"] = wfrest::Json::array();

    if (S_ISREG(file_stat.st_mode)) {
        task->t_compare_result_json["diffs"].emplace_back();
        ret = exe_sql_job_for_file(task,
                                   compare_file_path.c_str(),
                                   false/*not dir*/,
                                   task->t_compare_result_json["diffs"].back());
        LOG_CHECK_ERR_RETURN(ret);
    } else if (S_ISDIR(file_stat.st_mode)) {
        // 如果是目录，那么就需要一个文件一个文件地比，并且生成相应的结果
        ret = exe_sql_job_for_dir(task, compare_file_path.c_str());
    } else {
        LOG_ROOT_ERR(E_DC_COMPARE_EXE_TASK_FAILED,
                     "compare_file_path:%s is not a file or dir",
                     compare_file_path.c_str());
        return E_DC_COMPARE_EXE_TASK_FAILED;
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
        bool has_task = task_q_.read_once((void**)&task);
        if (!has_task) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        DC_COMMON_ASSERT(task != nullptr);

        ret = exe_sql_job(task);
        LOG_CHECK_ERR(ret);
        task->t_task_status = T_TASK_OVER;
        if (ret != S_SUCCESS) {
            dc_diff_failed_content_t diff { task };
            diff.gen_diff(task);
        }
    } // end while

    return S_SUCCESS;
}
