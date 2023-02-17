#include "dc_api_task.h"
#include "dc_common_assert.h"
#include "dc_common_error.h"

#include <unordered_map>
#include <algorithm>

static dc_common_code_t
build_task_server_info(cJSON *server_item,
                       dc_api_ctx_default_server_info_t *server_info)
{
    char msg_buf[4096];

    DC_COMMON_ASSERT(server_item != NULL);
    DC_COMMON_ASSERT(server_info != NULL);
    DC_COMMON_ASSERT(server_info->c_task != NULL);

#define HANDLE_ERROR_MSG(emsg, ...)           do {          \
        snprintf(msg_buf, sizeof(msg_buf), __VA_ARGS__);    \
        LOG_ROOT_ERR(E_ARG_INVALID, msg_buf);               \
        server_info->c_error = E_ARG_INVALID;               \
        server_info->c_error_msg = emsg;                    \
    } while (0)

    // get center name
    cJSON *server_name = cJSON_GetObjectItem(server_item, "center");
    if (server_name == NULL || 
        server_name->type != cJSON_String ||
        server_name->valuestring == NULL ||
        server_name->valuestring[0] == '\0') {
        HANDLE_ERROR_MSG("center name is invalid", 
                         "task:%s center name is invalid",
                         server_info->c_task->t_task_uuid.c_str());
        return E_ARG_INVALID;
    }
    server_info->c_center = server_name->valuestring;
    DC_COMMON_ASSERT(server_info->c_center.length() > 0);

    // get host name or ip address
    cJSON *server_hostname = cJSON_GetObjectItem(server_item, "host");
    if (server_hostname == NULL || 
        server_hostname->type != cJSON_String ||
        server_hostname->valuestring == NULL ||
        server_hostname->valuestring[0] == '\0') {
        HANDLE_ERROR_MSG("server host is invalid", 
                         "task:%s server host is invalid",
                         server_info->c_task->t_task_uuid.c_str());
        return E_ARG_INVALID;
    }
    server_info->c_host = server_hostname->valuestring;
    DC_COMMON_ASSERT(server_info->c_host.length() > 0);

    // get user
    cJSON *server_user = cJSON_GetObjectItem(server_item, "user");
    if (server_user == NULL ||
        server_user->type != cJSON_String ||
        server_user->valuestring == NULL ||
        server_user->valuestring[0] == '\0') {
        HANDLE_ERROR_MSG("user is invalid",
                         "task:%s given server's user is invalid",
                         server_info->c_task->t_task_uuid.c_str());
        return E_ARG_INVALID;
    }

    server_info->c_user = server_user->valuestring;
    DC_COMMON_ASSERT(server_info->c_user.length() > 0);

    // get password
    cJSON *server_password = cJSON_GetObjectItem(server_item, "password");
    if (server_password == NULL ||
        server_password->type != cJSON_String ||
        server_password->valuestring == NULL ||
        server_password->valuestring[0] == '\0') {
        HANDLE_ERROR_MSG("password is invalid",
                         "task:%s given server's password is invalid",
                         server_info->c_task->t_task_uuid.c_str());
        return E_ARG_INVALID;
    }

    server_info->c_password = server_password->valuestring;
    DC_COMMON_ASSERT(server_info->c_password.length() > 0);

    // get port number
    cJSON *server_port = cJSON_GetObjectItem(server_item, "port");
    if (server_port == NULL ||
        server_port->type != cJSON_Number ||
        server_port->valueint <= 0) {
        HANDLE_ERROR_MSG("port is invalid",
                         "task:%s given server's port is invalid",
                         server_info->c_task->t_task_uuid.c_str());
        return E_ARG_INVALID;
    }

    server_info->c_port = server_port->valueint;
    DC_COMMON_ASSERT(server_info->c_port > 0);

    // get is standard bool value
    cJSON *server_is_standard = cJSON_GetObjectItem(server_item, "standard");
    if (server_is_standard == NULL ||
        (server_is_standard->type != cJSON_True &&
        server_is_standard->type != cJSON_False)) {
        HANDLE_ERROR_MSG("is_standard is invalid",
                         "task:%s given server's standard is invalid",
                         server_info->c_task->t_task_uuid.c_str());
        return E_ARG_INVALID;
    }

    server_info->c_standard = server_is_standard->type == cJSON_True ? true : false;

    // get excluded file regex array
    cJSON *server_excluded_file_regex = cJSON_GetObjectItem(server_item, "excluded_file_regex");
    if (server_excluded_file_regex != NULL) {
        if (server_excluded_file_regex->type != cJSON_Array) {
            HANDLE_ERROR_MSG("excluded_file_regex is invalid",
                             "task:%s given server's excluded_file_regex is invalid",
                             server_info->c_task->t_task_uuid.c_str());
            return E_ARG_INVALID;
        }

        // get every item of file regex array
        cJSON *file_regex_item = NULL;
        cJSON_ArrayForEach(file_regex_item, server_excluded_file_regex) {
            if (file_regex_item->type != cJSON_String ||
                file_regex_item->valuestring == NULL ||
                file_regex_item->valuestring[0] == '\0') {
                HANDLE_ERROR_MSG("excluded_file_regex is invalid",
                                 "task:%s given server's excluded_file_regex is invalid",
                                 server_info->c_task->t_task_uuid.c_str());
                return E_ARG_INVALID;
            }

            server_info->c_excluded_file_regex.push_back(file_regex_item->valuestring);
        }
    }

    // get c_path_to_compare
    cJSON *server_path_to_compare = cJSON_GetObjectItem(server_item, "path_to_compare");
    if (server_path_to_compare == NULL ||
        server_path_to_compare->type != cJSON_String ||
        server_path_to_compare->valuestring == NULL ||
        server_path_to_compare->valuestring[0] == '\0') {
        HANDLE_ERROR_MSG("path_to_compare is invalid",
                         "task:%s given server's path_to_compare is invalid",
                         server_info->c_task->t_task_uuid.c_str());
        return E_ARG_INVALID;
    }
    server_info->c_path_to_compare = server_path_to_compare->valuestring;
    DC_COMMON_ASSERT(server_info->c_path_to_compare.length() > 0);

#undef HANDLE_ERROR_MSG

    return S_SUCCESS;
}

dc_common_code_t
build_task_from_json(const char *task_content,
                     const uint32_t task_content_len,
                     cJSON *root,
                     dc_api_task_t *task/*已经生成内存*/)
{
    char msg_buf[4096];

    DC_COMMON_ASSERT(task_content != NULL);
    DC_COMMON_ASSERT(task_content_len > 0);
    DC_COMMON_ASSERT(root != NULL);
    DC_COMMON_ASSERT(task != NULL);

#define HANDLE_ERROR_MSG(emsg, ...)           do {          \
        snprintf(msg_buf, sizeof(msg_buf), __VA_ARGS__);    \
        LOG_ROOT_ERR(E_ARG_INVALID, msg_buf);               \
        task->t_error = E_ARG_INVALID;                      \
        task->t_error_msg = emsg;                           \
    } while (0)

    cJSON *task_uuid = cJSON_GetObjectItem(root, "uuid");
    if (task_uuid == NULL || 
        task_uuid->type != cJSON_String ||
        task_uuid->valuestring == NULL) {
        HANDLE_ERROR_MSG("no task uuid provided",
                         "did not provide task_uuid, task_content:%.*s"
                         ,
                         task_content_len,
                         task_content);
        return E_ARG_INVALID;
    }

    task->t_task_uuid = task_uuid->valuestring;
    DC_COMMON_ASSERT(task->t_task_uuid.length() > 0);

    cJSON *result_type = cJSON_GetObjectItem(root, "result_type");
    task->t_result_type = FC_TASK_RESULT_GIT_DIFF;

    if (result_type != NULL) {
        if (result_type->type != cJSON_Number) {
            HANDLE_ERROR_MSG("result_type is not number",
                             "task:%s did not provide valid result_type, task_content:%.*s"
                             ,
                             task->t_task_uuid.c_str(),
                             task_content_len,
                             task_content);
            return E_ARG_INVALID;
        }

        if (!(result_type->valueint >= FC_TASK_RESULT_GIT_DIFF &&
              result_type->valueint < FC_TASK_RESULT_INVALID)) {
              HANDLE_ERROR_MSG("result_type value is not valid",
                               "task:%s did not provide valid result_type, task_content:%.*s"
                               ,
                               task->t_task_uuid.c_str(),
                               task_content_len,
                               task_content);
            return E_ARG_INVALID;
        }

        task->t_result_type = result_type->valueint;
    }

    DC_COMMON_ASSERT(task->t_result_type >= FC_TASK_RESULT_GIT_DIFF &&
                     task->t_result_type < FC_TASK_RESULT_INVALID);

    // excluded file regex
    cJSON *excluded_file_regex = cJSON_GetObjectItem(root, "excluded_file_regex");
    if (excluded_file_regex != NULL) {
        if (excluded_file_regex->type != cJSON_Array) {
            HANDLE_ERROR_MSG("excluded_file_regex is not array",
                             "task:%s did not provide valid excluded_file_regex, task_content:%.*s"
                             ,
                             task->t_task_uuid.c_str(),
                             task_content_len,
                             task_content);
            return E_ARG_INVALID;
        }
        // excluded_file_regex is an array, we need to fetch every item from
        // excluded_file_regex.
        cJSON *excluded_file_regex_item = NULL;
        cJSON_ArrayForEach(excluded_file_regex_item, excluded_file_regex) {
            if (excluded_file_regex_item->type != cJSON_String ||
                excluded_file_regex_item->valuestring == NULL) {
                HANDLE_ERROR_MSG("excluded_file_regex item is not string",
                                 "task:%s did not provide valid excluded_file_regex, task_content:%.*s"
                                 ,
                                 task->t_task_uuid.c_str(),
                                 task_content_len,
                                 task_content);
                return E_ARG_INVALID;
            }

            task->t_excluded_file_regex.push_back(excluded_file_regex_item->valuestring);
        }
    }

    // get servers list
    cJSON *servers = cJSON_GetObjectItem(root, "servers");
    if (servers == NULL || 
        servers->type != cJSON_Array) {
        HANDLE_ERROR_MSG("no servers_list provided",
                         "task:%s did not provide valid servers, task_content:%.*s"
                         ,
                         task->t_task_uuid.c_str(),
                         task_content_len,
                         task_content);
        return E_ARG_INVALID;
    }

    // servers is an array, we need to fetch every item from servers.
    cJSON *server_item = NULL;
    cJSON_ArrayForEach(server_item, servers) {
        if (server_item->type != cJSON_Object) {
            HANDLE_ERROR_MSG("server item is not object",
                             "task:%s did not provide valid servers, task_content:%.*s"
                             ,
                             task->t_task_uuid.c_str(),
                             task_content_len,
                             task_content);
            return E_ARG_INVALID;
        }

        dc_api_ctx_default_server_info_t server_info;
        server_info.c_task = task;
        dc_common_code_t ret = build_task_server_info(server_item, &server_info);
        LOG_CHECK_ERR_RETURN(ret);
        task->t_server_info_arr.push_back(server_info);
    }

    // find and set std_idx value
    task->t_std_idx = -1;
    for (size_t i = 0; i < task->t_server_info_arr.size(); i++) {
        if (task->t_server_info_arr[i].c_standard) {
            if (task->t_std_idx != -1) {
                LOG_ROOT_ERR(E_ARG_INVALID,
                             "task:%s has more than one standard server, task_content:%.*s"
                             ,
                             task->t_task_uuid.c_str(),
                             task_content_len,
                             task_content);
                return E_ARG_INVALID;
            }
            task->t_std_idx = i;
        }
    }

    if (task->t_std_idx == -1) {
        HANDLE_ERROR_MSG("no standard server provided",
                         "task:%s did not provide standard server, task_content:%.*s"
                         ,
                         task->t_task_uuid.c_str(),
                         task_content_len,
                         task_content);
        return E_ARG_INVALID;
    }

#undef HANDLE_ERROR_MSG
    return S_SUCCESS;
}

dc_common_code_t
build_compare_result_json(dc_api_task_t *task)
{
    DC_COMMON_ASSERT(task != NULL);

    std::unordered_map<std::string, std::vector<int>> result_map;
    std::vector<std::string> dir_name_list;
    auto &file_diff_list = task->t_fs_result;

    for (std::size_t i = 0; i < file_diff_list.size(); i++) {
        auto &file_diff = file_diff_list[i];
        result_map[file_diff.r_dir_path].push_back(i);
        dir_name_list.push_back(file_diff.r_dir_path);
    }

    // sort dir_name_list
    std::sort(dir_name_list.begin(), dir_name_list.end());

    cJSON *root = cJSON_CreateObject();
    // set id = task->t_uuid
    cJSON_AddStringToObject(root, "id", task->t_task_uuid.c_str());

    // add a "data" array
    cJSON *data = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "data", data);

    // iterate dir_name_list
    for (auto &dir_name : dir_name_list) {
        cJSON *dir_item = cJSON_CreateObject();
        cJSON_AddItemToArray(data, dir_item);

        cJSON_AddStringToObject(dir_item, "dir", dir_name.c_str());

        cJSON *file_diff_list = cJSON_CreateArray();
        cJSON_AddItemToObject(dir_item, "files", file_diff_list);

        for (auto &file_diff_idx : result_map[dir_name]) {
            auto &file_diff = task->t_fs_result[file_diff_idx];
            cJSON *file_diff_item = cJSON_CreateObject();
            cJSON_AddItemToArray(file_diff_list, file_diff_item);

            // set file name
            cJSON_AddStringToObject(file_diff_item, "file", file_diff.r_filename_no_dir_path.c_str());

            // add a "servers" array
            cJSON *servers = cJSON_CreateArray();
            cJSON_AddItemToObject(file_diff_item, "servers", servers);

            DC_COMMON_ASSERT(file_diff.r_server_diff_arr.size() > 0);
            // for single file, there are servers diff array list to iterate
            for (std::size_t i = 0; i < file_diff.r_server_diff_arr.size(); i++) {
                auto &server_diff = file_diff.r_server_diff_arr[i];
                cJSON *server_diff_item = cJSON_CreateObject();
                cJSON_AddItemToArray(servers, server_diff_item);

                // set server name
                cJSON_AddStringToObject(server_diff_item, "server", server_diff.d_center_name.c_str());
                // set size
                cJSON_AddNumberToObject(server_diff_item, "size", server_diff.d_file_size);
                // set permission
                cJSON_AddStringToObject(server_diff_item, "permission", server_diff.d_permission.c_str());
                // set owner
                cJSON_AddStringToObject(server_diff_item, "owner", server_diff.d_owner.c_str());
                // set last updated time
                cJSON_AddStringToObject(server_diff_item, "last_updated_time", server_diff.d_last_updated_time.c_str());
                // set diff file path
                cJSON_AddStringToObject(server_diff_item, "diff_file_path", server_diff.d_diff_file_path.c_str());
                // set file mode
                cJSON_AddStringToObject(server_diff_item, "file_mode", server_diff.d_file_mode.c_str());
            }
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    task->t_compare_result.clear();
    task->t_compare_result.assign(json_str);
    free(json_str);
    cJSON_Delete(root);

    return S_SUCCESS;
}