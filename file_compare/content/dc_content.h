#ifndef _DC_CONTENT_H_
#define _DC_CONTENT_H_

#include "dc_api_task.h"                // api_task_t
#include "dc_common_error.h"            // dc_error_t
#include "chan.h"                       // 消息通道!

#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <vector>
#include <thread>
#include <atomic>

typedef struct dc_file_attr {
    uint64_t f_size;
    mode_t f_mode;         // 文件的权限
    std::string f_ower;    // 文件拥有者
    time_t f_last_updated; // 文件最后更新时间
} dc_file_attr_t;

class dc_content_t {
  public:
    dc_content_t(dc_api_ctx_default_server_info_t *server) {}
    virtual ~dc_content_t() {}

  public:
    // 注意： 这些接口都是异步的！
    // 发送命令去获取整个文件的属性
    virtual dc_common_code_t do_file_attr(const std::string &path) = 0;
    // 异步获取文件的attr
    virtual dc_common_code_t get_file_attr(dc_file_attr_t *attr) = 0;

    // 发送命令去获取整个文件的每一行的sha1
    virtual dc_common_code_t do_file_content(const std::string &path) = 0;
    // 异步获取整个文件的每一行的sha1
    virtual dc_common_code_t get_file_content(std::vector<std::string> *content) = 0;
};

class dc_content_local_t : public dc_content_t {
  public:
    dc_content_local_t(dc_api_ctx_default_server_info_t *server);
    virtual ~dc_content_local_t();

  public:
    // 注意： 这些接口都是异步的！
    // 并且都是由主线程去调用!
    // 发送命令去获取整个文件的属性
    virtual dc_common_code_t do_file_attr(const std::string &path) override;
    // 异步获取文件的attr
    virtual dc_common_code_t get_file_attr(dc_file_attr_t *attr) override;

    // 发送命令去获取整个文件的每一行的sha1
    virtual dc_common_code_t do_file_content(const std::string &path) override;
    // 异步获取整个文件的每一行的sha1
    virtual dc_common_code_t get_file_content(std::vector<std::string> *content) override;

private:
    // worker_线程的主函数
    dc_common_code_t thd_worker();

    // worker_线程的读取文件的attr
    dc_common_code_t thd_worker_file_attr();

    // worker_线程的读取文件的每一行的sha1
    dc_common_code_t thd_worker_file_content();

  private:
    // cmd的类型
    enum {
        CMD_TYPE_NONE,
        CMD_TYPE_FILE_ATTR,
        CMD_TYPE_FILE_CONTENT,
    };

private:
    dc_api_ctx_default_server_info_t *server_ {nullptr};

    // 线程是否需要退出？
    std::atomic<bool> exit_ { false };

    // 异步线程，主要用来执行读取文件内容的操作
    std::thread *worker_ { nullptr };

    // 这里运的时候，必然已经是个文件
    // 这个文件的路径不是绝对路径，而是相对于server的路径
    // 所以还需要利用server中的路径来拼一下!
    std::string file_path_;

    // 通过这个消息管道向worker_发送命令!
    msg_chan_t cmd_q_;

    // 通过这个消息管道完成worker_异步返回值的接收!
    msg_chan_t ret_q_;

    // worker_线程读取文件之后，获得的文件的属性
    dc_file_attr_t file_attr_;
};

class dc_content_remote_t : public dc_content_t {
  public:
    dc_content_remote_t(dc_api_ctx_default_server_info_t *server);
    virtual ~dc_content_remote_t();

  public:
    // 注意： 这些接口都是异步的！
    // 发送命令去获取整个文件的属性
    virtual dc_common_code_t do_file_attr(const std::string &path) override;
    // 异步获取文件的attr
    virtual dc_common_code_t get_file_attr(dc_file_attr_t *attr) override;

    // 发送命令去获取整个文件的每一行的sha1
    virtual dc_common_code_t do_file_content(const std::string &path) override;
    // 异步获取整个文件的每一行的sha1
    virtual dc_common_code_t get_file_content(std::vector<std::string> *content) override;
};

#endif /* ! _DC_CONTENT_H_ */