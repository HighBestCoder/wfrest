#include "dc_content.h"
#include "dc_common_error.h"
#include "dc_common_log.h"
#include "dc_common_trace_log.h"

#include <openssl/sha.h>                // sha1

#include <string.h>

#include <sys/types.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define DC_CONTENT_MEM_ALIGN            8192                // 8K
#define DC_CONTENT_FILE_READ_BUF        16777216            // 读文件的缓冲区大小 16M

dc_content_local_t::dc_content_local_t(dc_api_ctx_default_server_info_t *server) :
                    dc_content_t(server)
{
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

    DC_COMMON_ASSERT(worker_ == nullptr);

    // free 2 buffers
    if (file_read_buf_ != nullptr) {
        free(file_read_buf_);
        file_read_buf_ = nullptr;
    }

    DC_COMMON_ASSERT(file_read_buf_ == nullptr);
    DC_COMMON_ASSERT(file_read_fd_ == -1);
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
        return E_DC_CONTENT_RETRY;
    }

    LOG_CHECK_ERR_RETURN((dc_common_code_t)ret);
    return S_SUCCESS;
}

dc_common_code_t
dc_content_local_t::do_file_content(const std::string &path,
                                    std::vector<std::string> *lines_sha1 /*OUT*/,
                                    int *empty_lines /*OUT*/)
{
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

dc_common_code_t
dc_content_local_t::get_file_content()
{
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

        if (ret_code == S_SUCCESS) {
            // 读成功了!
            file_read_state_ = FILE_CONTENT_READ_OVER;
            return ret_code;
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

    DC_COMMON_ASSERT(0 == "not handle status");

    return E_DC_CONTENT_RETRY;
}

dc_common_code_t
dc_content_local_t::thd_worker()
{
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

dc_common_code_t
dc_content_local_t::thd_worker_file_attr()
{
    // 读取文件属性
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(file_path_.size() > 0);
    DC_COMMON_ASSERT(file_attr_ != nullptr);

    // 读取文件属性
    // 首先stat查看一下文件是否存在?
    struct stat file_stat;
    int stat_ret = stat(file_path_.c_str(), &file_stat);
    if (stat_ret != 0) {
        LOG_ROOT_ERR(E_OS_ENV_STAT,
                     "stat file failed, file_path=%s, errno=%d, errstr=%s",
                     file_path_.c_str(),
                     errno,
                     strerror(errno));
        return E_OS_ENV_STAT;
    }

    // if the file is a directory
    // return error, here just support file
    if (S_ISDIR(file_stat.st_mode)) {
        LOG_ROOT_ERR(E_DC_CONTENT_DIR,
                     "file is a directory, file_path=%s",
                     file_path_.c_str());
        return E_DC_CONTENT_DIR;
    }

    // get file size
    file_attr_->f_size = file_stat.st_size;

    // get file mode
    file_attr_->f_mode = file_stat.st_mode;

    // get owner name by st_uid
    struct passwd *pwd = getpwuid(file_stat.st_uid);
    if (pwd == nullptr) {
        LOG_ROOT_ERR(E_OS_ENV_GETPWUID,
                     "getpwuid failed, errno=%d, errstr=%s",
                     errno,
                     strerror(errno));
        return E_OS_ENV_GETPWUID;
    }

    file_attr_->f_owner = pwd->pw_name;

    // get file last updated time
    file_attr_->f_last_updated = file_stat.st_mtime;

    return S_SUCCESS;
}

dc_common_code_t
dc_content_local_t::thd_worker_append_to_pre_line(const uint8_t *s, const int len)
{
    if (len == 0) {
        return S_SUCCESS;
    }

    DC_COMMON_ASSERT(s != nullptr);
    DC_COMMON_ASSERT(len > 0);

    file_pre_line_.append((const char *)s, len);

    return S_SUCCESS;
}

bool
dc_content_local_t::thd_worker_check_is_empty_line(const uint8_t *s, const int len)
{
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

dc_common_code_t
dc_content_local_t::thd_worker_compute_sha1_of_single_line(const uint8_t *s, const int len)
{
    const uint8_t *str_to_compute = s;
    int str_to_compute_len = len;

    DC_COMMON_ASSERT(s != nullptr);
    DC_COMMON_ASSERT(len >= 0);

    if (!file_pre_line_.empty()) {
        // append to line
        thd_worker_append_to_pre_line(s, len);
        str_to_compute = (const uint8_t*)file_pre_line_.c_str();
        str_to_compute_len = file_pre_line_.length();
    }

    if (thd_worker_check_is_empty_line(str_to_compute, str_to_compute_len)) {
        // empty line, skip
        file_pre_line_.clear();
        (*empty_lines_)++;
        return S_SUCCESS;
    }

    lines_sha1_->emplace_back(SHA_DIGEST_LENGTH, 0);
    SHA1(str_to_compute, str_to_compute_len, (unsigned char*)&(lines_sha1_->back())[0]);
    file_pre_line_.clear();

    return S_SUCCESS;
}

dc_common_code_t
dc_content_local_t::compute_sha1_by_lines(const int buf_len)
{
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

dc_common_code_t
dc_content_local_t::thd_worker_file_content_prepare(void)
{
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(file_path_.size() > 0);
    DC_COMMON_ASSERT(lines_sha1_ != nullptr);
    DC_COMMON_ASSERT(lines_sha1_->size() == 0);
    DC_COMMON_ASSERT(empty_lines_ != nullptr);
    DC_COMMON_ASSERT(*empty_lines_ == 0);

    // here we need to alloc a posize aligned 8K buffer
    // to read file content.
    if (file_read_buf_ == nullptr) {
        if (posix_memalign((void **)&file_read_buf_,
                           DC_CONTENT_MEM_ALIGN,
                           DC_CONTENT_FILE_READ_BUF) != 0) {
            LOG_ROOT_ERR(E_OS_ENV_MEM,
                        "posix_memalign failed, errno=%d, errstr=%s",
                        errno,
                        strerror(errno));
            return E_OS_ENV_MEM;
        }
    }
    DC_COMMON_ASSERT(file_read_buf_ != nullptr);

    DC_COMMON_ASSERT(file_read_fd_ == -1);

    file_read_fd_ = open(file_path_.c_str(), O_RDONLY | O_DIRECT);
    if (file_read_fd_ < 0) {
        LOG_ROOT_ERR(E_OS_ENV_OPEN,
                     "open file failed, file_path=%s, errno=%d, errstr=%s",
                     file_path_.c_str(),
                     errno,
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
dc_common_code_t
dc_content_local_t::thd_worker_file_content_read(void)
{
    dc_common_code_t ret = S_SUCCESS;

    DC_COMMON_ASSERT(file_path_.size() > 0);
    DC_COMMON_ASSERT(lines_sha1_ != nullptr);
    DC_COMMON_ASSERT(empty_lines_ != nullptr);

    //DC_COMMON_ASSERT(file_read_state_ == FILE_CONTENT_READ_PREPAR);
    DC_COMMON_ASSERT(file_read_fd_ >= 0);

    // read 16MB from file
    int read_len = read(file_read_fd_,
                        file_read_buf_,
                        DC_CONTENT_FILE_READ_BUF);
    if (read_len < 0) {
        LOG_ROOT_ERR(E_OS_ENV_READ,
                     "read file failed, file_path=%s, errno=%d, errstr=%s",
                     file_path_.c_str(),
                     errno,
                     strerror(errno));
        return E_OS_ENV_READ;
    }

    if (read_len == 0) {
        // we have read all the file content
        if (!file_pre_line_.empty()) {
            // we MUST compute the sha1 of the pre_line
            // because the pre_line may not be empty
            // and we need to compute the sha1 of the
            // pre_line and the current line
            // and then clear the pre_line.
            // so we need to copy the current line to
            // the pre_line
            if (thd_worker_check_is_empty_line((const uint8_t*)file_pre_line_.c_str(), file_pre_line_.length())) {
                (*empty_lines_)++;
            } else {
                lines_sha1_->emplace_back(SHA_DIGEST_LENGTH, 0);
                SHA1((const uint8_t*)file_pre_line_.c_str(),
                     file_pre_line_.length(),
                     (unsigned char*)&(lines_sha1_->back())[0]);
                file_pre_line_.clear();
            }
        }

        // 没有内容还需要处理!
        DC_COMMON_ASSERT(read_len == 0);

        // close fd
        close(file_read_fd_);
        file_read_fd_ = -1;

        return E_DC_CONTENT_OVER;
    }

    if (read_len < DC_CONTENT_FILE_READ_BUF) {
        // we have read all the file content
        compute_sha1_by_lines(read_len);

        // we have read all the file content
        if (!file_pre_line_.empty()) {
            // we MUST compute the sha1 of the pre_line
            // check is empty line
            if (thd_worker_check_is_empty_line((const uint8_t*)file_pre_line_.c_str(),
                                               file_pre_line_.length())) {
                (*empty_lines_)++;
            } else {
                lines_sha1_->emplace_back(SHA_DIGEST_LENGTH, 0);
                SHA1((const uint8_t*)file_pre_line_.c_str(),
                     file_pre_line_.length(),
                     (unsigned char*)&(lines_sha1_->back())[0]);
                file_pre_line_.clear();
            }
        }

        // close fd
        close(file_read_fd_);
        file_read_fd_ = -1;

        return E_DC_CONTENT_OVER;
    }

    DC_COMMON_ASSERT(read_len == DC_CONTENT_FILE_READ_BUF);

    compute_sha1_by_lines(read_len);

    return S_SUCCESS;
}