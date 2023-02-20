#include "dc_internal_task.h"
#include "dc_common_assert.h"
#include "dc_common_error.h"
#include "dc_common_trace_log.h"

#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>

static dc_common_code_t
dc_internal_task_run_path_attr(const std::string &path,
                               wfrest::Json &json_file_attr)
{
    DC_COMMON_ASSERT(!path.empty());

    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        LOG_ROOT_ERR(E_OS_ENV_STAT,
                     "stat failed, path:%s, errno:%d, error:%s",
                     path.c_str(), errno, strerror(errno));
        json_file_attr["path"] = path;
        json_file_attr["errno"] = errno;
        json_file_attr["error"] = strerror(errno);
        return E_OS_ENV_STAT;
    }

    json_file_attr["type"] = st.st_mode & S_IFMT;
    json_file_attr["path"] = path;
    if ((st.st_mode & S_IFMT) == S_IFREG) {
        json_file_attr["size"] = st.st_size;
    }
    json_file_attr["mode"] = st.st_mode;

    // change st.st_uid to Linux OS username by getpwuid
    struct passwd *pwd = getpwuid(st.st_uid);
    DC_COMMON_ASSERT(pwd != nullptr);
    json_file_attr["owner"] = pwd->pw_name;

    // convert st_mtime to string
    char time_str[64] = {0};
    struct tm *tm = localtime(&st.st_mtime);
    DC_COMMON_ASSERT(tm != nullptr);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm);
    json_file_attr["last_updated"] = time_str;

    return S_SUCCESS;
}

static dc_common_code_t
dc_internal_task_run_dir_scan(const std::string &path, wfrest::Json &json_obj_list)
{
    DC_COMMON_ASSERT(!path.empty());

    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) {
        LOG_ROOT_ERR(E_OS_ENV_OPEN,
                     "opendir failed, path:%s, errno:%d, error:%s",
                     path.c_str(), errno, strerror(errno));
        wfrest::Json json;
        json["path"] = path;
        json["errno"] = errno;
        json["error"] = strerror(errno);
        json_obj_list.push_back(json);
        return E_OS_ENV_OPEN;
    }

    // push current dir attr
    wfrest::Json dir_json;
    dc_common_code_t ret = dc_internal_task_run_path_attr(path, dir_json);
    LOG_CHECK_ERR_RETURN(ret);
    json_obj_list.push_back(dir_json);

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        // new path
        std::string new_path = path + "/" + entry->d_name;
        // check new path is dir or file
        if (entry->d_type == DT_DIR) {
            dc_common_code_t ret = dc_internal_task_run_dir_scan(new_path, json_obj_list);
            LOG_CHECK_ERR_RETURN(ret);
        } else if (entry->d_type == DT_REG) {
            wfrest::Json file_json;
            dc_common_code_t ret = dc_internal_task_run_path_attr(new_path, file_json);
            LOG_CHECK_ERR_RETURN(ret);
            json_obj_list.push_back(file_json);
        }
    }

    closedir(dir);
    return S_SUCCESS;
}

dc_common_code_t
dc_internal_task_run(dc_interal_task_t *task /* CHANGED */)
{
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(task != nullptr);
    DC_COMMON_ASSERT(!task->i_uuid.empty());
    DC_COMMON_ASSERT(!task->i_path.empty());

    const auto &path = task->i_path;
    auto &out_json = task->i_json;

    // 注意，这里不放目录
    out_json["uuid"] = task->i_uuid;

    // 所有的结果都放到这里来!
    out_json["results"] = wfrest::Json::array();

    // check path is a file or dir
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        LOG_ROOT_ERR(E_OS_ENV_STAT,
                     "stat failed, path:%s, errno:%d, error:%s",
                     path.c_str(), errno, strerror(errno));
        // 如果出错，那么out_json["results"]里面就是空的!
        out_json["path"] = path;
        out_json["errno"] = errno;
        out_json["error"] = strerror(errno);
        return E_OS_ENV_STAT;
    }

    if ((st.st_mode & S_IFMT) == S_IFREG) {
        wfrest::Json json_file_attr;
        ret = dc_internal_task_run_path_attr(path, json_file_attr);
        LOG_CHECK_ERR_RETURN(ret);
        out_json["results"].push_back(json_file_attr);
    } else if ((st.st_mode & S_IFMT) == S_IFDIR) {
        ret = dc_internal_task_run_dir_scan(path, out_json["results"]);
        LOG_CHECK_ERR_RETURN(ret);
    }

    return ret;
}