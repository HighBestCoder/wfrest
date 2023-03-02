#include "dc_content.h"

#include <fcntl.h>
#include <openssl/sha.h>  // sha1
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "dc_common_error.h"
#include "dc_common_log.h"
#include "dc_common_trace_log.h"
#include "wfrest/Compress.h"
#include "wfrest/ErrorCode.h"
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"
#include "workflow/WFFacilities.h"
#include "workflow/WFTaskFactory.h"

#define DC_CONTENT_MEM_ALIGN 8192          // 8K
#define DC_CONTENT_FILE_READ_BUF 16777216  // 读文件的缓冲区大小 16M

dc_content_local_t::dc_content_local_t(dc_api_ctx_default_server_info_t *server) : dc_content_t(server) {
    server_ = server;

    // 创建一个线程去处理命令
    worker_ = new std::thread(&dc_content_local_t::thd_worker, this);
    DC_COMMON_ASSERT(worker_ != nullptr);

    memset(md5_, 0, sizeof(md5_));
}

dc_content_local_t::~dc_content_local_t() {
    exit_ = true;

    if (worker_ != nullptr) {
        worker_->join();
        delete worker_;
        worker_ = nullptr;
    }

    DC_COMMON_ASSERT(worker_ == nullptr);

    // free 2 buffers
    if (file_read_buf_ != nullptr) {
        free(file_read_buf_);
        file_read_buf_ = nullptr;
    }

    DC_COMMON_ASSERT(file_read_buf_ == nullptr);

    if (file_read_fd_ != -1) {
        LOG(DC_COMMON_LOG_ERROR, "[JIYOU] file_read_fd_:%d is not closed, file:%s", file_read_fd_, file_path_.c_str());
    }

    DC_COMMON_ASSERT(file_read_fd_ == -1);
}

dc_common_code_t dc_content_local_t::do_file_attr(const std::string &path, dc_file_attr_t *attr /*OUT*/) {
    // 注意下面两个顺序不能乱!
    // 不能先发送消息再设置file_path_
    // 先设置file_path_
    file_path_ = path;
    file_attr_ = attr;
    // 然后发送命令
    cmd_q_.write(CMD_TYPE_FILE_ATTR);

    return S_SUCCESS;
}

dc_common_code_t dc_content_local_t::get_file_attr(void) {
    bool has_read_msg = false;
    int ret = 0;

    has_read_msg = ret_q_.read_once(&ret);
    if (!has_read_msg) {
        return E_DC_CONTENT_RETRY;
    }

    LOG(DC_COMMON_LOG_INFO, "center:%s return attr ret code:%d", server_->c_center.c_str(), ret);

    LOG_CHECK_ERR_RETURN((dc_common_code_t)ret);
    return S_SUCCESS;
}

dc_common_code_t dc_content_local_t::do_dir_list_attr(const std::vector<std::string> *dir_list,
                                                      std::vector<dc_file_attr_t> *attr_list /*OUT*/) {
    // 注意下面两个顺序不能乱!
    // 不能先发送消息再设置file_path_
    // 先设置file_path_
    dir_attr_list_ = attr_list;
    dir_list_ = dir_list;
    // 然后发送命令
    cmd_q_.write(CMD_TYPE_DIR_LIST_ATTR);

    return S_SUCCESS;
}
dc_common_code_t dc_content_local_t::get_dir_list_attr() {
    bool has_read_msg = false;
    int ret = 0;

    has_read_msg = ret_q_.read_once(&ret);
    if (!has_read_msg) {
        return E_DC_CONTENT_RETRY;
    }

    LOG(DC_COMMON_LOG_INFO, "center:%s return dir_attr ret code:%d", server_->c_center.c_str(), ret);

    LOG_CHECK_ERR_RETURN((dc_common_code_t)ret);
    return S_SUCCESS;
}

dc_common_code_t dc_content_local_t::do_file_content(const std::string &path,
                                                     std::vector<std::string> *lines_sha1 /*OUT*/,
                                                     int *empty_lines /*OUT*/) {
    // 注意下面两个顺序不能乱!
    // 不能先发送消息再设置file_path_
    // 先设置file_path_
    DC_COMMON_ASSERT(file_read_state_ == FILE_CONTENT_READ_INIT);

    file_path_ = path;
    lines_sha1_ = lines_sha1;
    empty_lines_ = empty_lines;
    // 然后发送命令
    cmd_q_.write(CMD_TYPE_FILE_CONTENT_PREPARE);
    file_read_state_ = FILE_CONTENT_READ_PREPAR;

    DC_COMMON_ASSERT(file_read_state_ == FILE_CONTENT_READ_PREPAR);

    return S_SUCCESS;
}

dc_common_code_t dc_content_local_t::get_file_content() {
    bool has_read_msg = false;
    int ret = 0;

    // 如果处在刚prepare好的状态
    if (file_read_state_ == FILE_CONTENT_READ_PREPAR) {
        has_read_msg = ret_q_.read_once(&ret);
        if (!has_read_msg) {
            return E_DC_CONTENT_RETRY;
        }

        // 如果读到了消息, 则说明prepare已经完成
        // 1. 这里prepare出错了
        LOG_CHECK_ERR_RETURN((dc_common_code_t)ret);

        // 2. prepare成功了
        // 那么需要发送读命令
        cmd_q_.write(CMD_TYPE_FILE_CONTENT_READ);
        file_read_state_ = FILE_CONTENT_READ_DO;

        // 这个时候因为没有行返回，所以还是要重试!
        return E_DC_CONTENT_RETRY;
    }

    // 已经发送过了读命令
    if (file_read_state_ == FILE_CONTENT_READ_DO) {
        has_read_msg = ret_q_.read_once(&ret);
        if (!has_read_msg) {
            return E_DC_CONTENT_RETRY;
        }

        dc_common_code_t ret_code = (dc_common_code_t)ret;

        // ret_code == S_SUCCESS
        // 只是表明这次读成功了，但是文件可能还需要再接着下一次读!
        if (ret_code == S_SUCCESS) {
            // 读成功了!
            // 把状态改成：当前这次读已经读完了
            // 注意！这个状态并不是说整个文件的内容已经读完了!
            file_read_state_ = FILE_CONTENT_READ_OVER;
            // 返回的值表示还要继续读
            return E_DC_CONTENT_RETRY;
        }

        if (ret_code == E_DC_CONTENT_OVER) {
            // 读文件结束了!
            DC_COMMON_ASSERT(file_read_fd_ == -1);
            file_read_state_ = FILE_CONTENT_READ_INIT;
            return E_DC_CONTENT_OVER;
        }

        // 读文件出错了!
        return ret_code;
    }

    // 又需要读文件了
    if (file_read_state_ == FILE_CONTENT_READ_OVER) {
        // 那么需要发送读命令
        cmd_q_.write(CMD_TYPE_FILE_CONTENT_READ);
        file_read_state_ = FILE_CONTENT_READ_DO;

        // 这个时候因为没有行返回，所以还是要重试!
        return E_DC_CONTENT_RETRY;
    }

    LOG(DC_COMMON_LOG_ERROR, "[JIYOU] file_read_state_:%d", file_read_state_);

    DC_COMMON_ASSERT(0 == "not handle status");

    return E_DC_CONTENT_RETRY;
}

dc_common_code_t dc_content_local_t::thd_worker() {
    int cmd = CMD_TYPE_NONE;
    bool has_cmd = false;
    dc_common_code_t ret = S_SUCCESS;

    while (!exit_) {
        has_cmd = cmd_q_.read_once(&cmd);
        if (has_cmd) {
            if (cmd == CMD_TYPE_FILE_ATTR) {
                // file_attr只会往ret_q_写入一次消息
                ret = thd_worker_file_attr();
                LOG_CHECK_ERR(ret);
                ret_q_.write((int)ret);
            } else if (cmd == CMD_TYPE_DIR_LIST_ATTR) {
                // dir_list_attr只会往ret_q_写入一次消息
                ret = thd_worker_dir_list_attr();
                LOG_CHECK_ERR(ret);
                ret_q_.write((int)ret);
            } else if (cmd == CMD_TYPE_FILE_CONTENT_PREPARE) {
                ret = thd_worker_file_content_prepare();
                LOG_CHECK_ERR(ret);
                ret_q_.write((int)ret);
            } else if (cmd == CMD_TYPE_FILE_CONTENT_READ) {
                ret = thd_worker_file_content_read();
                LOG_CHECK_ERR(ret);
                ret_q_.write((int)ret);
            } else {
                DC_COMMON_ASSERT(false);
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return S_SUCCESS;
}

dc_common_code_t dc_content_local_t::thd_worker_file_attr() {
    // 读取文件属性
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(file_path_.size() > 0);
    DC_COMMON_ASSERT(file_attr_ != nullptr);

    // 读取文件属性
    // 首先stat查看一下文件是否存在?
    struct stat file_stat;
    int stat_ret = stat(file_path_.c_str(), &file_stat);
    if (stat_ret != 0) {
        LOG_ROOT_ERR(E_OS_ENV_STAT, "stat file failed, file_path=%s, errno=%d, errstr=%s", file_path_.c_str(), errno,
                     strerror(errno));
        return E_OS_ENV_STAT;
    }

    // if the file is a directory
    // return error, here just support file
    if (S_ISDIR(file_stat.st_mode)) {
        file_attr_->f_size = 0;
    } else {
        // get file size
        file_attr_->f_size = file_stat.st_size;
    }

    // get file mode, then convert st_mode to string
    char mode_str[32] = {0};
    snprintf(mode_str, sizeof(mode_str), "%o", file_stat.st_mode & 0777);
    file_attr_->f_mode = mode_str;

    // get owner name by st_uid
    struct passwd *pwd = getpwuid(file_stat.st_uid);
    if (pwd == nullptr) {
        LOG_ROOT_ERR(E_OS_ENV_GETPWUID, "getpwuid failed, errno=%d, errstr=%s", errno, strerror(errno));
        return E_OS_ENV_GETPWUID;
    }

    LOG(DC_COMMON_LOG_INFO, "begin to set owner of file:%s", file_path_.c_str());
    file_attr_->f_owner = pwd->pw_name;
    LOG(DC_COMMON_LOG_INFO, "set owner of file:%s, owner:%s", file_path_.c_str(), file_attr_->f_owner.c_str());

    // get file last updated time, convert st_mtime to string
    char time_str[32] = {0};
    struct tm *tm = localtime(&file_stat.st_mtime);
    if (tm == nullptr) {
        LOG_ROOT_ERR(E_OS_ENV_NOT_FOUND, "localtime failed, errno=%d, errstr=%s", errno, strerror(errno));
        return E_OS_ENV_NOT_FOUND;
    }
    // format tm to string
    strftime(time_str, sizeof(time_str), "%Y%m%d%-H%M%S", tm);
    file_attr_->f_last_updated = time_str;

    // set a memory barrier here
    std::atomic_thread_fence(std::memory_order_release);

    return ret;
}

dc_common_code_t dc_content_local_t::thd_worker_dir_list_attr(void) {
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(dir_list_ != nullptr);
    DC_COMMON_ASSERT(dir_attr_list_ != nullptr);

    if (dir_list_->empty()) {
        return ret;
    }

    for (auto &dir : *dir_list_) {
        dir_attr_list_->emplace_back();
        auto &dir_attr = dir_attr_list_->back();

        // 读取文件属性
        // 首先stat查看一下文件是否存在?
        struct stat file_stat;
        int stat_ret = stat(dir.c_str(), &file_stat);
        if (stat_ret != 0) {
            LOG_ROOT_ERR(E_OS_ENV_STAT, "stat file failed, file_path=%s, errno=%d, errstr=%s", file_path_.c_str(),
                         errno, strerror(errno));
            dir_attr.f_code = E_OS_ENV_STAT;
            continue;
        }

        // if the file is a directory
        // return error, here just support file
        DC_COMMON_ASSERT(S_ISDIR(file_stat.st_mode));

        // get file mode, then convert st_mode to string
        char mode_str[128] = {0};
        snprintf(mode_str, sizeof(mode_str), "%o", file_stat.st_mode & 0777);
        dir_attr.f_mode = mode_str;

        // get owner name by st_uid
        struct passwd *pwd = getpwuid(file_stat.st_uid);
        if (pwd == nullptr) {
            if (errno == 0) {
                dir_attr.f_owner = "0";
            } else {
                LOG_ROOT_ERR(E_OS_ENV_GETPWUID, "task:%s path:%s getpwuid failed, errno=%d, errstr=%s",
                             server_->c_task->t_task_uuid.c_str(), dir.c_str(), errno, strerror(errno));
                dir_attr.f_code = E_OS_ENV_GETPWUID;
                continue;
            }
        } else {
            dir_attr.f_owner = pwd->pw_name;
        }

        // get file last updated time, convert st_mtime to string
        char time_str[32] = {0};
        struct tm *tm = localtime(&file_stat.st_mtime);
        if (tm == nullptr) {
            LOG_ROOT_ERR(E_OS_ENV_LOCALTIME, "localtime failed, errno=%d, errstr=%s", errno, strerror(errno));
            dir_attr.f_code = E_OS_ENV_LOCALTIME;
            continue;
        }

        // format tm to string
        strftime(time_str, sizeof(time_str), "%Y%m%d%-H%M%S", tm);
        dir_attr.f_last_updated = time_str;
    }

    return ret;
}

void dc_content_local_t::thd_worker_clear_pre_line(void) {
    if (file_pre_line_.length() > DC_CONTENT_FILE_READ_BUF) {
        // use swap to free memory of file_pre_line_
        std::string temp;
        file_pre_line_.swap(temp);
    } else {
        file_pre_line_.clear();
    }
}

dc_common_code_t dc_content_local_t::thd_worker_append_to_pre_line(const uint8_t *s, const int len) {
    if (len == 0) {
        return S_SUCCESS;
    }

    DC_COMMON_ASSERT(s != nullptr);
    DC_COMMON_ASSERT(len > 0);
    DC_COMMON_ASSERT(len <= DC_CONTENT_FILE_READ_BUF);

    file_pre_line_.append((const char *)s, len);

    return S_SUCCESS;
}

bool dc_content_local_t::thd_worker_check_is_empty_line(const uint8_t *s, const int len) {
    int i = 0;

    DC_COMMON_ASSERT(s != nullptr);
    DC_COMMON_ASSERT(len >= 0);

    for (i = 0; i < len; i++) {
        if (!isspace(s[i])) {
            return false;
        }
    }

    return true;
}

dc_common_code_t dc_content_local_t::thd_worker_compute_sha1_of_single_line(const uint8_t *s, const int len) {
    const uint8_t *str_to_compute = s;
    int str_to_compute_len = len;

    DC_COMMON_ASSERT(s != nullptr);
    DC_COMMON_ASSERT(len >= 0);

    if (!file_pre_line_.empty()) {
        // append to line
        thd_worker_append_to_pre_line(s, len);
        str_to_compute = (const uint8_t *)file_pre_line_.c_str();
        str_to_compute_len = file_pre_line_.length();
    }

    if (thd_worker_check_is_empty_line(str_to_compute, str_to_compute_len)) {
        thd_worker_clear_pre_line();
        (*empty_lines_)++;
        return S_SUCCESS;
    }

    lines_sha1_->emplace_back(SHA_DIGEST_LENGTH, 0);
    SHA1(str_to_compute, str_to_compute_len, (unsigned char *)&(lines_sha1_->back())[0]);
    thd_worker_clear_pre_line();

    return S_SUCCESS;
}

dc_common_code_t dc_content_local_t::compute_sha1_by_lines(const int buf_len) {
    int i = 0;
    int cur_line_begin = 0;
    dc_common_code_t ret;

    DC_COMMON_ASSERT(buf_len > 0);

    for (i = 0; i < buf_len; i++) {
        char c = file_read_buf_[i];
        if (c == '\n') {
            // 读到了一行[cur_line_begin, i)
            // 计算sha1
            DC_COMMON_ASSERT(i >= cur_line_begin);
            ret = thd_worker_compute_sha1_of_single_line(file_read_buf_ + cur_line_begin, i - cur_line_begin);
            LOG_CHECK_ERR_RETURN(ret);
            cur_line_begin = i + 1;
        }
    }

    // 如果还余下一点尾巴
    // 需要append到line里面
    if (cur_line_begin < buf_len) {
        ret = thd_worker_append_to_pre_line(file_read_buf_ + cur_line_begin, buf_len - cur_line_begin);
        LOG_CHECK_ERR_RETURN(ret);
    }

    return S_SUCCESS;
}

dc_common_code_t dc_content_local_t::thd_worker_file_content_prepare(void) {
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(file_path_.size() > 0);
    DC_COMMON_ASSERT(lines_sha1_ != nullptr);
    DC_COMMON_ASSERT(lines_sha1_->size() == 0);
    DC_COMMON_ASSERT(empty_lines_ != nullptr);
    DC_COMMON_ASSERT(*empty_lines_ == 0);

    // here we need to alloc a posize aligned 8K buffer
    // to read file content.
    if (file_read_buf_ == nullptr) {
        if (posix_memalign((void **)&file_read_buf_, DC_CONTENT_MEM_ALIGN, DC_CONTENT_FILE_READ_BUF) != 0) {
            LOG_ROOT_ERR(E_OS_ENV_MEM, "posix_memalign failed, errno=%d, errstr=%s", errno, strerror(errno));
            return E_OS_ENV_MEM;
        }
    }
    DC_COMMON_ASSERT(file_read_buf_ != nullptr);

    DC_COMMON_ASSERT(file_read_fd_ == -1);

    file_read_fd_ = open(file_path_.c_str(), O_RDONLY | O_DIRECT);
    if (file_read_fd_ < 0) {
        LOG_ROOT_ERR(E_OS_ENV_OPEN, "open file failed, file_path=%s, errno=%d, errstr=%s", file_path_.c_str(), errno,
                     strerror(errno));
        return E_OS_ENV_OPEN;
    }

    return S_SUCCESS;
}

/*
 * 这个函数只是一个简单的执行器。
 * 所以并不清楚上层的逻辑
 * 只有3种返回值
 * 1. 读出错
 * 2. 读到了文件尾
 * 3. 读出了DC_CONTENT_FILE_READ_BUF
 */
dc_common_code_t dc_content_local_t::thd_worker_file_content_read(void) {
    DC_COMMON_ASSERT(file_path_.size() > 0);
    DC_COMMON_ASSERT(lines_sha1_ != nullptr);
    DC_COMMON_ASSERT(empty_lines_ != nullptr);

    // DC_COMMON_ASSERT(file_read_state_ == FILE_CONTENT_READ_PREPAR);
    DC_COMMON_ASSERT(file_read_fd_ >= 0);

    // read 16MB from file
    int read_len = read(file_read_fd_, file_read_buf_, DC_CONTENT_FILE_READ_BUF);
    if (read_len < 0) {
        LOG_ROOT_ERR(E_OS_ENV_READ, "read file failed, file_path=%s, errno=%d, errstr=%s", file_path_.c_str(), errno,
                     strerror(errno));
        return E_OS_ENV_READ;
    }

    // update md5 value of md5_buf
    MD5(file_read_buf_, read_len, md5_);

    if (read_len == 0) {
        // we have read all the file content
        if (!file_pre_line_.empty()) {
            // we MUST compute the sha1 of the pre_line
            // because the pre_line may not be empty
            // and we need to compute the sha1 of the
            // pre_line and the current line
            // and then clear the pre_line. So we need to
            // copy the current line to the pre_line.
            if (thd_worker_check_is_empty_line((const uint8_t *)file_pre_line_.c_str(), file_pre_line_.length())) {
                (*empty_lines_)++;
            } else {
                lines_sha1_->emplace_back(SHA_DIGEST_LENGTH, 0);
                SHA1((const uint8_t *)file_pre_line_.c_str(), file_pre_line_.length(),
                     (unsigned char *)&(lines_sha1_->back())[0]);
                thd_worker_clear_pre_line();
            }
        }

        // 没有内容还需要处理!
        DC_COMMON_ASSERT(read_len == 0);

        // close fd
        close(file_read_fd_);
        file_read_fd_ = -1;
        LOG(DC_COMMON_LOG_INFO, "read file finished, file_path=%s", file_path_.c_str());

        return E_DC_CONTENT_OVER;
    }

    if (read_len < DC_CONTENT_FILE_READ_BUF) {
        // we have read all the file content
        compute_sha1_by_lines(read_len);

        // we have read all the file content
        if (!file_pre_line_.empty()) {
            // we MUST compute the sha1 of the pre_line
            // check is empty line
            if (thd_worker_check_is_empty_line((const uint8_t *)file_pre_line_.c_str(), file_pre_line_.length())) {
                (*empty_lines_)++;
            } else {
                lines_sha1_->emplace_back(SHA_DIGEST_LENGTH, 0);
                SHA1((const uint8_t *)file_pre_line_.c_str(), file_pre_line_.length(),
                     (unsigned char *)&(lines_sha1_->back())[0]);
                thd_worker_clear_pre_line();
            }
        }

        // close fd
        LOG(DC_COMMON_LOG_INFO, "[JIYOU] read file finished, file_path=%s", file_path_.c_str());
        close(file_read_fd_);
        file_read_fd_ = -1;

        return E_DC_CONTENT_OVER;
    }

    DC_COMMON_ASSERT(read_len == DC_CONTENT_FILE_READ_BUF);

    compute_sha1_by_lines(read_len);

    return S_SUCCESS;
}

dc_common_code_t dc_content_local_t::get_file_md5(std::string &out) {
    // convert the md5_ to print the md5 value
    char md5_str[MD5_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        snprintf(md5_str + i * 2, 3, "%02x", md5_[i]);
    }
    md5_str[MD5_DIGEST_LENGTH * 2] = '\0';
    out = md5_str;
    return S_SUCCESS;
}

// 注意： 这些接口都是异步的！
// 发送命令去获取整个文件的属性
dc_common_code_t dc_content_remote_t::do_file_attr(const std::string &path, dc_file_attr_t *attr /*OUT*/) {
    return E_NOT_IMPL;
}
// 异步获取文件的attr
dc_common_code_t dc_content_remote_t::get_file_attr(void) { return E_NOT_IMPL; }

// 发送命令去获取整个文件的每一行的sha1
dc_common_code_t dc_content_remote_t::do_file_content(const std::string &path,
                                                      std::vector<std::string> *lines_sha1 /*OUT*/,
                                                      int *empty_lines /*OUT*/) {
    return E_NOT_IMPL;
}

// 异步获取整个文件的每一行的sha1
dc_common_code_t dc_content_remote_t::get_file_content() { return E_NOT_IMPL; }

dc_common_code_t dc_content_remote_t::get_file_md5(std::string &md5_out) { return E_NOT_IMPL; }

void http_post_callback(WFHttpTask *task) {
    const void *body;
    size_t body_len;
    int ret = task->get_resp()->get_parsed_body(&body, &body_len);
    DC_COMMON_ASSERT(ret == wfrest::StatusOK);

    http_task_user_data_t *user_data = (http_task_user_data_t *)task->user_data;
    std::string resp_body;
    ret = wfrest::Compressor::ungzip(static_cast<const char *>(body), body_len, &resp_body);
    DC_COMMON_ASSERT(ret == wfrest::StatusOK);

    wfrest::Json json = wfrest::Json::parse(resp_body);
    // check has error message ?
    DC_COMMON_ASSERT(json.find("error") != json.end());
    // get error from json
    std::string error = json["error"];
    // convert to dc_common_code_t
    dc_common_code_t code = (dc_common_code_t)std::stoi(error);

    msg_chan_t *out_q = user_data->out_q;
    DC_COMMON_ASSERT(out_q != nullptr);
    out_q->write(code);
}

int http_post_task(const std::string &url, const std::string &body, http_task_user_data_t *user_data) {
    WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                       /*redirect_max*/ 4,
                                                       /*retry_max*/ 2, http_post_callback);

    std::string zip_body;
    int ret = wfrest::Compressor::gzip(&body, &zip_body);
    DC_COMMON_ASSERT(ret == wfrest::StatusOK);

    task->user_data = user_data;  // 指向ctx，后面会delete
    task->get_req()->set_method("POST");
    task->get_req()->add_header_pair("Content-Encoding", "gzip");
    task->get_req()->add_header_pair("Content-Type", "application/json");
    task->get_req()->append_output_body_nocopy(zip_body.c_str(), zip_body.size());
    task->start();

    return 0;
}

dc_common_code_t dc_content_remote_t::do_dir_list_attr(const std::vector<std::string> *dir_list,
                                                       std::vector<dc_file_attr_t> *attr_list /*OUT*/) {
    DC_COMMON_ASSERT(dir_list != nullptr);
    DC_COMMON_ASSERT(attr_list != nullptr);

    attr_list_ = attr_list;

    // build request body to get dirs' attr
    wfrest::Json body_json;
    body_json["uuid"] = server_->c_task->t_task_uuid;
    body_json["base_dir"] = server_->c_base_dir;
    body_json["dirs"] = wfrest::Json::array();
    for (auto &dir : *dir_list) {
        body_json["dirs"].push_back(dir);
    }

    // after build the request body, we need to send the request
    std::string url = "http://" + server_->c_host + ":" + std::to_string(server_->c_port) + "/internal/dir/task/" +
                      server_->c_task->t_task_uuid;
    std::string body = body_json.dump();

    http_user_data_.out_q = &dir_list_out_q_;
    http_user_data_.get_resp_body = nullptr;

    http_post_task(url, body, &http_user_data_);
    get_dir_list_status_ = DIR_LIST_STATUS_INIT;

    return S_SUCCESS;
}

static void http_get_callback(WFHttpTask *task) {
    const void *body;
    size_t body_len;
    int ret = task->get_resp()->get_parsed_body(&body, &body_len);
    DC_COMMON_ASSERT(ret == wfrest::StatusOK);

    http_task_user_data_t *user_data = (http_task_user_data_t *)task->user_data;
    std::string resp_body;
    ret = wfrest::Compressor::ungzip(static_cast<const char *>(body), body_len, &resp_body);
    DC_COMMON_ASSERT(ret == wfrest::StatusOK);

    wfrest::Json json = wfrest::Json::parse(resp_body);
    // check has error message ?
    DC_COMMON_ASSERT(json.find("error") != json.end());
    // get error from json
    std::string error = json["error"];
    // convert to dc_common_code_t
    dc_common_code_t code = (dc_common_code_t)std::stoi(error);

    if (code == S_SUCCESS && user_data->get_resp_body != nullptr) {
        user_data->get_resp_body->swap(resp_body);
    }

    msg_chan_t *out_q = user_data->out_q;
    DC_COMMON_ASSERT(out_q != nullptr);

    out_q->write(code);
}

int http_get_task(const std::string &url, http_task_user_data_t *user_data) {
    WFHttpTask *task = WFTaskFactory::create_http_task(url,
                                                       /*redirect_max*/ 4,
                                                       /*retry_max*/ 2, http_get_callback);
    task->user_data = user_data;
    task->get_req()->set_method("GET");
    task->get_req()->add_header_pair("Content-Type", "application/json");
    task->start();
    return 0;
}

dc_common_code_t dc_content_remote_t::get_dir_list_attr() {
    int ret = S_SUCCESS;

    // 结束之后不可以再call
    DC_COMMON_ASSERT(get_dir_list_status_ != DIR_LIST_STATUS_OVER);

    // 如果只是发送了请求给server，那么就需要等待server的回复
    if (get_dir_list_status_ == DIR_LIST_STATUS_INIT) {
        // 这里去查看一下是否给了回复

        auto has_msg = dir_list_out_q_.read_once(&ret);
        if (!has_msg) {
            return E_DC_CONTENT_RETRY;
        }

        // 如果有回复，那么就需要看一下ret的值
        // 这里在请求的时候，是不可能返回retry的。
        // 也就是说，当不成功的时候，就返回了失败!
        if (ret != S_SUCCESS) {
            return (dc_common_code_t)ret;
        }

        // 说明接收方已经成功接收到了!
        // 开始发送get请求，去尝试拿一下dir_list的attr
        // 这里发送get请求
        std::string url = "http://" + server_->c_host + ":" + std::to_string(server_->c_port) + "/internal/dir/task/" +
                          server_->c_task->t_task_uuid;

        http_user_data_.out_q = &dir_list_out_q_;
        http_user_data_.get_resp_body = &get_resp_body_;

        http_get_task(url, &http_user_data_);

        // 表示已经发送了get命令!
        get_dir_list_status_ = DIR_LIST_STATUS_SEND;

        return E_DC_CONTENT_RETRY;
    } else if (get_dir_list_status_ == DIR_LIST_STATUS_SEND) {
        auto has_msg = dir_list_out_q_.read_once(&ret);
        if (!has_msg) {
            return E_DC_CONTENT_RETRY;
        }

        // 如果有回复，那么就需要看一下ret的值
        if (ret != S_SUCCESS) {
            // 如果get的时候，返回的消息是retry
            // 那么这个时候，就需要重新发送get请求
            // 因为服务方可能还没有把这个任务做完!
            if (ret == E_DC_CONTENT_RETRY) {
                std::string url = "http://" + server_->c_host + ":" + std::to_string(server_->c_port) +
                                  "/internal/dir/task/" + server_->c_task->t_task_uuid;

                http_user_data_.out_q = &dir_list_out_q_;
                http_user_data_.get_resp_body = &get_resp_body_;

                http_get_task(url, &http_user_data_);

                return E_DC_CONTENT_RETRY;
            }

            // 服务方返回其他错误!
            return (dc_common_code_t)ret;
        }

        // 说明接收方已经成功接收到resp_body
        // 开始解析resp_body
        // 并且把结果放到attr_list中
        wfrest::Json json = wfrest::Json::parse(get_resp_body_);
        DC_COMMON_ASSERT(json.find("error") != json.end());

        std::string error = json["error"];
        // server在处理get的时候出错了!
        dc_common_code_t code = (dc_common_code_t)std::stoi(error);
        if (code != S_SUCCESS) {
            return code;
        }

        DC_COMMON_ASSERT(json.find("result") != json.end());
        DC_COMMON_ASSERT(json.find("uuid") != json.end());
        DC_COMMON_ASSERT(json["uuid"] == server_->c_task->t_task_uuid);

        wfrest::Json result = json["result"];
        // iterate result json array, then conver the result to attr_list
        for (auto &item : result) {
            dc_file_attr_t attr;
            // because this is dir, so, no size should set.
            attr.f_mode = item["mode"];
            attr.f_owner = item["owner"];
            attr.f_last_updated = item["last_updated"];
            attr.f_code = item["code"];

            attr_list_->push_back(attr);
        }

        get_dir_list_status_ = DIR_LIST_STATUS_OVER;
        return S_SUCCESS;
    }

    DC_COMMON_ASSERT(0 == "should not goes to here");

    return E_NOT_IMPL;
}