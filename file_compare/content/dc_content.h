#ifndef _DC_CONTENT_H_
#define _DC_CONTENT_H_

#include "dc_api_task.h"                // api_task_t
#include "dc_common_error.h"            // dc_error_t

#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <vector>

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

    // 发送命令去获取整个文件的属性
    virtual dc_common_code_t do_file_attr(const std::string &path) override;
    // 异步获取文件的attr
    virtual dc_common_code_t get_file_attr(dc_file_attr_t *attr) override;

    // 发送命令去获取整个文件的每一行的sha1
    virtual dc_common_code_t do_file_content(const std::string &path) override;
    // 异步获取整个文件的每一行的sha1
    virtual dc_common_code_t get_file_content(std::vector<std::string> *content) override;

private:
    dc_api_ctx_default_server_info_t *server_ {nullptr};
};

class dc_content_remote_t : public dc_content_t {
  public:
    dc_content_remote_t(dc_api_ctx_default_server_info_t *server);
    virtual ~dc_content_remote_t();

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