#include <iostream>

#include "dc_common_assert.h"
#include "dc_common_error.h"
#include "dc_content.h"
#include "dc_internal_task.h"

// include wfrest::Json header
#include "wfrest/json.hpp"

int main(void) {
    wfrest::Json json = wfrest::Json::array();
    std::vector<std::string> dir_list{"abc", "def", "ghi"};

    // put dir_list into json
    for (auto &dir : dir_list) {
        json.push_back(dir);
    }

    dir_list.clear();

    // get items from json
    for (auto &item : json) {
        std::cout << item << std::endl;
        dir_list.push_back(item);
    }

    // print dir_list
    for (auto &dir : dir_list) {
        std::cout << dir << std::endl;
    }
    return 0;
}