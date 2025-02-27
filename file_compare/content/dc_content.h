#ifndef _DC_CONTENT_H_
#define _DC_CONTENT_H_

#include <fcntl.h>
#include <openssl/md5.h>
#include <sys/stat.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "chan.h"             // 消息通道!
#include "dc_api_task.h"      // api_task_t
#include "dc_common_error.h"  // dc_error_t

typedef struct dc_file_attr {
    int64_t f_size{-1};                  // 文件大小
    std::string f_mode;                  // 文件的权限
    std::string f_owner;                 // 文件拥有者
    std::string f_last_updated;          // 文件最后更新时间
    dc_common_code_t f_code{S_SUCCESS};  // 文件的错误码

    bool compare(const dc_file_attr &f) const {
        return (f_code == 0 && f.f_code == 0 && f_size == f.f_size && f_mode == f.f_mode && f_owner == f.f_owner &&
                f_last_updated == f.f_last_updated);
    }
} dc_file_attr_t;

// 接口类
// 1. 读取文件属性
// 2. 计算文件每一行的sha1
//    注意：在计算的时候，不要把每一行结束的\n\r也算到行内容里面!
class dc_content_t {
public:
    dc_content_t(dc_api_ctx_default_server_info_t *server) {}
    virtual ~dc_content_t() {}

public:
    // 注意： 这些接口都是异步的！
    // 发送命令去获取整个文件的属性
    virtual dc_common_code_t do_file_attr(const std::string &path, dc_file_attr_t *attr /*OUT*/) = 0;
    // 异步获取文件的attr
    virtual dc_common_code_t get_file_attr(void) = 0;

    // 发送命令
    virtual dc_common_code_t do_file_content(const std::string &path, std::vector<std::string> *lines_sha1 /*OUT*/,
                                             int *empty_lines /*OUT*/) = 0;
    // 异步获取整个文件的每一行的sha1
    virtual dc_common_code_t get_file_content() = 0;

    // 整个文件的md5
    virtual dc_common_code_t get_file_md5(std::string &md5_out) = 0;

    virtual dc_common_code_t do_dir_list_attr(const std::vector<std::string> *dir_list,
                                              std::vector<dc_file_attr_t> *attr_list /*OUT*/) = 0;
    virtual dc_common_code_t get_dir_list_attr() = 0;
};

class dc_content_local_t : public dc_content_t {
public:
    dc_content_local_t(dc_api_ctx_default_server_info_t *server);
    virtual ~dc_content_local_t();

public:
    // 注意： 这些接口都是异步的！
    // 并且都是由主线程去调用!
    // 发送命令去获取整个文件的属性
    virtual dc_common_code_t do_file_attr(const std::string &path, dc_file_attr_t *attr /*OUT*/) override;
    // 异步获取文件的attr
    virtual dc_common_code_t get_file_attr(void) override;

    // 发送命令去获取整个文件的每一行的sha1
    // 这里只会发送prepare命令，真正的读取文件内容的命令是在get_file_content中发送的!
    virtual dc_common_code_t do_file_content(const std::string &path, std::vector<std::string> *lines_sha1 /*OUT*/,
                                             int *empty_lines /*OUT*/) override;
    // 异步获取整个文件的每一行的sha1
    virtual dc_common_code_t get_file_content(void) override;

    // TODO md5这里有问题!是要原始文件的md5还是我算出来的md5?
    virtual dc_common_code_t get_file_md5(std::string &md5_out) override;

    virtual dc_common_code_t do_dir_list_attr(const std::vector<std::string> *dir_list,
                                              std::vector<dc_file_attr_t> *attr_list /*OUT*/) override;
    virtual dc_common_code_t get_dir_list_attr() override;

private:
    const std::vector<std::string> *dir_list_{nullptr};
    std::vector<dc_file_attr_t> *dir_attr_list_{nullptr};

private:
    // worker_线程的主函数
    dc_common_code_t thd_worker();

    // worker_线程的读取文件的attr
    dc_common_code_t thd_worker_file_attr();

    // worker_线程的读取目录列表的属性
    dc_common_code_t thd_worker_dir_list_attr();

    // 做一些准备工作，比如打开文件，申请内存!
    dc_common_code_t thd_worker_file_content_prepare();

    // worker_线程的读取文件的每一行的sha1
    // 返回1表示遇到了空行
    // 返回0表示计算了sha1
    void thd_worker_clear_pre_line();
    dc_common_code_t thd_worker_append_to_pre_line(const uint8_t *s, const int len);
    bool thd_worker_check_is_empty_line(const uint8_t *s, const int len);
    dc_common_code_t thd_worker_compute_sha1_of_single_line(const uint8_t *s, const int len);
    dc_common_code_t compute_sha1_by_lines(const int read_size);
    dc_common_code_t thd_worker_file_content_read();

private:
    // cmd的类型
    enum {
        CMD_TYPE_NONE,
        CMD_TYPE_FILE_ATTR,
        CMD_TYPE_DIR_LIST_ATTR,
        CMD_TYPE_FILE_CONTENT_PREPARE,
        CMD_TYPE_FILE_CONTENT_READ
    };

    enum {
        // 读文件时的两个状态!
        FILE_CONTENT_READ_INIT,
        FILE_CONTENT_READ_PREPAR,
        FILE_CONTENT_READ_OVER,  // 读取16MB内容结束
        FILE_CONTENT_READ_DO
    };

    uint8_t *file_read_buf_{nullptr};
    std::string file_pre_line_;
    int file_read_fd_{-1};

    // get函数现在处在什么状态?
    // 只能是前面的READ_INIT或者READ_DO
    int file_read_state_{FILE_CONTENT_READ_INIT};

private:
    dc_api_ctx_default_server_info_t *server_{nullptr};

    // 线程是否需要退出？
    std::atomic<bool> exit_{false};

    // 异步线程，主要用来执行读取文件内容的操作
    std::thread *worker_{nullptr};

    // 这里运的时候，必然已经是个文件
    // 这个文件的路径不是绝对路径，而是相对于server的路径
    // 所以还需要利用server中的路径来拼一下!
    std::string file_path_;
    uint64_t file_read_pos_{0};

    // 通过这个消息管道向worker_发送命令!
    msg_chan_t cmd_q_;

    // 通过这个消息管道完成worker_异步返回值的接收!
    msg_chan_t ret_q_;

    // worker_线程读取文件之后，获得的文件的属性
    // 这些都是指向主线程中的变量的指针
    dc_file_attr_t *file_attr_{nullptr};
    std::vector<std::string> *lines_sha1_{nullptr};
    int *empty_lines_{nullptr};
    unsigned char md5_[MD5_DIGEST_LENGTH];
};

typedef struct http_task_user_data {
    msg_chan_t *out_q{nullptr};
    std::string *get_resp_body{nullptr};
} http_task_user_data_t;

class dc_content_remote_t : public dc_content_t {
public:
    dc_content_remote_t(dc_api_ctx_default_server_info_t *server) : dc_content_t(server) { server_ = server; }
    virtual ~dc_content_remote_t() {}

public:
    // 注意： 这些接口都是异步的！
    // 发送命令去获取整个文件的属性
    virtual dc_common_code_t do_file_attr(const std::string &path, dc_file_attr_t *attr /*OUT*/) override;
    // 异步获取文件的attr
    virtual dc_common_code_t get_file_attr(void) override;

    // 发送命令去获取整个文件的每一行的sha1
    virtual dc_common_code_t do_file_content(const std::string &path, std::vector<std::string> *lines_sha1 /*OUT*/,
                                             int *empty_lines /*OUT*/) override;

public:
    // 异步获取整个文件的每一行的sha1
    virtual dc_common_code_t get_file_content() override;

    virtual dc_common_code_t get_file_md5(std::string &md5_out) override;

public:  // 获取dir_list的属性
    virtual dc_common_code_t do_dir_list_attr(const std::vector<std::string> *dir_list,
                                              std::vector<dc_file_attr_t> *attr_list /*OUT*/) override;

    // get的时候，要分为两步：
    // 1. 拿到do_dir_list_attr发出去的请求
    // 2. 再发去get请求看看执行是否成功?
    // 所以这里需要一个简单的状态机。
    virtual dc_common_code_t get_dir_list_attr() override;

private:
    msg_chan_t dir_list_out_q_;
    http_task_user_data_t http_user_data_;
    std::vector<dc_file_attr_t> *attr_list_{nullptr};

    enum { DIR_LIST_STATUS_INIT, DIR_LIST_STATUS_SEND, DIR_LIST_STATUS_OVER };
    int get_dir_list_status_{DIR_LIST_STATUS_OVER};
    std::string get_resp_body_;

private:
    dc_api_ctx_default_server_info_t *server_{nullptr};
};

#endif /* ! _DC_CONTENT_H_ */