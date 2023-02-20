#ifndef _DC_INTERNAL_TASK_H_
#define _DC_INTERNAL_TASK_H_

#include "workflow/WFFacilities.h"
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"

#include "dc_content.h"
#include "dc_common_error.h"

#include <string>
#include <vector>

typedef struct dc_interal_task {
    // input
    std::string i_uuid;
    std::string i_path;

    // output
    // 这里存放的是一个列表，没有目录的层级结构
    // 每个obj都是两部分，一个是文件名，一个是文件的attr
    wfrest::Json i_json;
} dc_interal_task_t;

dc_common_code_t
dc_internal_task_run(dc_interal_task_t *task /* CHANGED */);


#endif /* _DC_INTERNAL_TASK_H_ */