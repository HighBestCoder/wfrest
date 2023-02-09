#include "fc_task.h"
#include "dc_common_assert.h"
#include "dc_common_error.h"

static dc_common_code_t
build_task_server_info(cJSON *server_item,
                       fc_task_server_info_t *server_info)
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
    cJSON *server_name = cJSON_GetObjectItem(server_item, "name");
    if (server_name == NULL || 
        server_name->type != cJSON_String ||
        server_name->valuestring == NULL ||
        server_name->valuestring[0] == '\0') {
        HANDLE_ERROR_MSG("server name is invalid", 
                         "task:%s server name is invalid",
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
    cJSON *server_is_standard = cJSON_GetObjectItem(server_item, "is_standard");
    if (server_is_standard == NULL ||
        server_is_standard->type != cJSON_True ||
        server_is_standard->type != cJSON_False) {
        HANDLE_ERROR_MSG("is_standard is invalid",
                         "task:%s given server's is_standard is invalid",
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

    // get compare result file path
    cJSON *server_compare_result_file_path = cJSON_GetObjectItem(server_item, "compare_result_file_path");
    if (server_compare_result_file_path == NULL ||
        server_compare_result_file_path->type != cJSON_String ||
        server_compare_result_file_path->valuestring == NULL ||
        server_compare_result_file_path->valuestring[0] == '\0') {
        HANDLE_ERROR_MSG("compare_result_file_path is invalid",
                         "task:%s given server's compare_result_file_path is invalid",
                         server_info->c_task->t_task_uuid.c_str());
        return E_ARG_INVALID;
    }
    server_info->c_compare_result_file_path = server_compare_result_file_path->valuestring;
    DC_COMMON_ASSERT(server_info->c_compare_result_file_path.length() > 0);

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

    return S_SUCCESS;
}

dc_common_code_t
build_task_from_json(const char *task_content,
                     const uint32_t task_content_len,
                     cJSON *root,
                     fc_task_task_t *task/*已经生成内存*/)
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

    cJSON *task_uuid = cJSON_GetObjectItem(root, "task_uuid");
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

        fc_task_server_info_t server_info;
        server_info.c_task = task;
        dc_common_code_t ret = build_task_server_info(server_item, &server_info);
        LOG_CHECK_ERR_RETURN(ret);
        task->t_server_list.push_back(server_info);
    }

    return S_SUCCESS;
}