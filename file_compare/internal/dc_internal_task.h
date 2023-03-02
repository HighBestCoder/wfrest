#ifndef _DC_INTERNAL_TASK_H_
#define _DC_INTERNAL_TASK_H_

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dc_common_error.h"
#include "dc_content.h"
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"
#include "workflow/WFFacilities.h"

enum {
    DC_INTERNAL_TASK_TYPE_DIR_ATTR = 0,
};

typedef struct dc_interal_task {
    std::string i_uuid;
    int i_task_type{DC_INTERNAL_TASK_TYPE_DIR_ATTR};
    int i_task_status{T_TASK_INIT};
    // input:
    // 这里存放的是一个列表，没有目录的层级结构
    // 每个obj都是两部分，一个是文件名，一个是文件的attr
    wfrest::Json i_json;

    wfrest::Json i_out_json;
} dc_internal_task_t;

class dc_internal_executor_t {
public:
    dc_internal_executor_t();
    virtual ~dc_internal_executor_t();

    dc_common_code_t add(dc_internal_task_t task);
    dc_common_code_t get(const std::string &uuid, wfrest::Json &result_json /*OUT*/);

    // TODO 完成功能，就是在其他server上多出来的文件的属性需要罗列!

private:
    // 任务：获取目录列表的属性
    void thd_worker_task_dir_attr_list(dc_internal_task_t &task);

    void thd_worker_set_task_over(dc_internal_task_t &task);
    bool thd_worker_has_task_to_run();
    dc_internal_task_t &thd_worker_pick_task_to_run();
    void thd_worker_execute();

private:
    std::atomic<bool> exit_;
    std::thread *thd_worker_{nullptr};
    std::mutex mutex_;
    std::condition_variable cond_;
    std::list<dc_internal_task_t> task_list_;
};

#endif /* _DC_INTERNAL_TASK_H_ */