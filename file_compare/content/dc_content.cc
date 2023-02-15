#include "dc_content.h"
#include "dc_common_error.h"
#include "dc_common_log.h"
#include "dc_common_trace_log.h"

#include <sys/types.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

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
        return E_DC_CONTENT_RETRY;
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

    // 这个函数里面也会检测exit_
    // 如果exit_为true, 则会直接返回
    ret = thd_worker_file_content();
    LOG_CHECK_ERR(ret);
    ret_q_.write(ret);

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
dc_content_local_t::thd_worker_file_content(void)
{
    dc_common_code_t ret = S_SUCCESS;
    constexpr int buf_size = 4096;

    DC_COMMON_ASSERT(file_path_.size() > 0);
    DC_COMMON_ASSERT(lines_sha1_ != nullptr);

    // 读取文件内容
    // 读取文件内容
    // here we need to alloc a posize aligned 4K buffer
    // to read file content.
    char *buf = nullptr;
    if (posix_memalign((void **)&buf, buf_size, buf_size) != 0) {
        LOG_ROOT_ERR(E_OS_ENV_MEM,
                     "posix_memalign failed, errno=%d, errstr=%s",
                     errno,
                     strerror(errno));
        return E_OS_ENV_MEM;
    }
    DC_COMMON_ASSERT(buf != nullptr);

    // clear the buf
    memset(buf, 0, buf_size);

    int fd = open(file_path_.c_str(), O_RDONLY | O_DIRECT);
    if (fd < 0) {
        LOG_ROOT_ERR(E_OS_ENV_OPEN,
                     "open file failed, file_path=%s, errno=%d, errstr=%s",
                     file_path_.c_str(),
                     errno,
                     strerror(errno));
        free(buf);
        return E_OS_ENV_OPEN;
    }

    // read 4K file content
    ssize_t read_size = read(fd, buf, buf_size);
    if (read_size < 0) {
        LOG_ROOT_ERR(E_OS_ENV_READ,
                     "read file failed, file_path=%s, errno=%d, errstr=%s",
                     file_path_.c_str(),
                     errno,
                     strerror(errno));
        free(buf);
        close(fd);
        return E_OS_ENV_READ;
    }

    // file size is empty
    if (read_size == 0) {
        return S_SUCCESS;
    }

    if (read_size < buf_size) {
        // this is the last piece of file content
        
    }



    return S_SUCCESS;
}