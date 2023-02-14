#include "dc_content.h"
#include "dc_common_error.h"
#include "dc_common_log.h"

dc_content_local_t::dc_content_local_t(dc_api_ctx_default_server_info_t *server) : dc_content_t(server) {
    server_ = server;
}

dc_common_code_t
dc_content_local_t::do_file_attr(const std::string &path,
                                 dc_file_attr_t *attr /*OUT*/)
{
    // 注意下面两个顺序不能乱!
    // 不能先发送消息再设置file_path_
    // 先设置file_path_
    file_path_ = path;
    // 然后发送命令
    cmd_q_.write(CMD_TYPE_FILE_ATTR);

    return S_SUCCESS;
}

dc_common_code_t
dc_content_local_t::get_file_attr(void)
{
    bool has_read_msg = false;
    int ret = 0;

    has_read_msg = ret_q_.read_once(&ret);
    if (!has_read_msg) {
        return E_DC_DB_RETRY;
    }

    LOG_CHECK_ERR_RETURN(ret);

    return S_SUCCESS;
}

dc_common_code_t
dc_content_local_t::do_file_content(const std::string &path)
{

}

dc_common_code_t
dc_content_local_t::get_file_content(std::vector<std::string> *content)
{

}