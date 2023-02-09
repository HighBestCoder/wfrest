#ifndef _FC_TASK_H_
#define _FC_TASK_H_

#include "dc_common_error.h"
#include "cJSON.h"

#include <string>
#include <vector>
#include <atomic>

struct fc_task_task;

typedef struct fc_task_server_info {
    fc_task_task *c_task;                                   // 任务的指针

    // input part
    std::string c_center;                                   // 只是个描述字段，用来说明哪个数据中心
    std::string c_host;                                     // = 连接file agent需要的ip
    std::string c_user;                                     // = 连接file agent需要的用户名
    std::string c_password;                                 // = 连接file agent需要的密码
    int c_port;                                             // = file agent的端口
    bool c_standard;                                        // 是否是比较的标准方?
    std::vector<std::string> c_excluded_file_regex;         // 有哪些文件会被排除
    std::string c_compare_result_file_path;                 // 比较结果的文件路径
    std::string c_path_to_compare;                          // 需要比较的路径
    // output part
    dc_common_code_t c_error;                               // 是否出错？
    std::string c_error_msg;                                // 出错的原因
    bool c_same;                                            // 是否相同？
    struct timeval c_begin_time;                            // 开始时间
    struct timeval c_end_time;                              // 结束时间
    uint64_t c_total_time_ms;                               // 总共花费的时间
    uint64_t c_diff_rows;                                   // 不同的行数
} fc_task_server_info_t;

enum {
    FC_TASK_RESULT_GIT_DIFF = 0,
    FC_TASK_RESULT_INVALID,
};

typedef struct fc_task_task {
    // input part
    std::string t_task_uuid;                                // 任务的uuid
    std::vector<fc_task_server_info_t> t_server_list;       // 有哪些服务器会参与比较
    int t_result_type;                                      // 输出diff的类型?
    std::vector<std::string> t_excluded_file_regex;         // 有哪些文件会被排除
    int t_std_idx;                                          // 标准方的索引
    // output part
    std::atomic<int> t_task_status;                         // 任务的状态
    dc_common_code_t t_error;                               // 是否出错？
    std::string t_error_msg;                                // 错误信息
    uint64_t t_diff_rows;                                   // 不同的行数
    uint64_t t_printed_bytes;
    uint64_t t_printed_diff_rows;
} fc_task_task_t;

dc_common_code_t
build_task_from_json(const char *task_content,
                     const uint32_t task_content_len,
                     cJSON *root,
                     fc_task_task_t *task/*已经生成内存*/);

#endif /* ! _FC_TASK_H_ */