#include "dc_internal_task.h"

#include "dc_content.h"
#include "dc_common_error.h"
#include "dc_common_assert.h"

#include <iostream>

int
main(void)
{
    std::string path = "/tmp";

    dc_interal_task_t task;
    task.i_path = path;
    task.i_uuid = "1234567890";

    auto ret = dc_internal_task_run(&task);
    DC_COMMON_ASSERT(ret == S_SUCCESS);

    // print the jon of task
    std::cout << task.i_json.dump(4) << std::endl;

    return 0;
}