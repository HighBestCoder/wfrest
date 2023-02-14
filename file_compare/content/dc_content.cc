#include "dc_content.h"
#include "dc_common_error.h"
#include "dc_common_log.h"
#include "dc_common_trace_log.h"

dc_content_local_t::dc_content_local_t(dc_api_ctx_default_server_info_t *server) : dc_content_t(server) {
    server_ = server;

    // 创建一个线程去处理命令
    worker_ = new std::thread(&dc_content_local_t::thd_worker, this);
    DC_COMMON_ASSERT(worker_ != nullptr);
}

dc_content_local_t::~dc_content_local_t() {
    exit_ = true;

    if (worker_ != nullptr) {
        worker_->join();
        delete worker_;
        worker_ = nullptr;
    }
}

dc_common_code_t
dc_content_local_t::do_file_attr(const std::string &path,
                                 dc_file_attr_t *attr /*OUT*/)
{
    // 注意下面两个顺序不能乱!
    // 不能先发送消息再设置file_path_
    // 先设置file_path_
    file_path_ = path;
    file_attr_ = attr;
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

    LOG_CHECK_ERR_RETURN((dc_common_code_t)ret);
    return S_SUCCESS;
}

dc_common_code_t
dc_content_local_t::do_file_content(const std::string &path,
                                    std::vector<std::string> *lines_sha1 /*OUT*/)
{
    // 注意下面两个顺序不能乱!
    // 不能先发送消息再设置file_path_
    // 先设置file_path_
    file_path_ = path;
    lines_sha1_ = lines_sha1;
    // 然后发送命令
    cmd_q_.write(CMD_TYPE_FILE_ATTR);

    return S_SUCCESS;
}

dc_common_code_t
dc_content_local_t::get_file_content()
{
    bool has_read_msg = false;
    int ret = 0;

    has_read_msg = ret_q_.read_once(&ret);
    if (!has_read_msg) {
        return E_DC_DB_RETRY;
    }

    LOG_CHECK_ERR_RETURN((dc_common_code_t)ret);
    return S_SUCCESS;
}

dc_common_code_t
dc_content_local_t::thd_worker()
{
    int cmd = CMD_TYPE_NONE;
    bool has_cmd = false;

    do {
        has_cmd = cmd_q_.read_once(&cmd);
        if (has_cmd) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } while (!has_cmd && !exit_);

    DC_COMMON_ASSERT(cmd == CMD_TYPE_FILE_ATTR);

    dc_common_code_t ret = thd_worker_file_attr();
    LOG_CHECK_ERR(ret);
    ret_q_.write(ret);

    has_cmd = false;
    do {
        has_cmd = cmd_q_.read_once(&cmd);
        if (has_cmd) {
            break;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    } while (!has_cmd && !exit_);

    DC_COMMON_ASSERT(cmd == CMD_TYPE_FILE_CONTENT);
    ret = thd_worker_file_content();
    LOG_CHECK_ERR(ret);
    ret_q_.write(ret);

    return S_SUCCESS;
}