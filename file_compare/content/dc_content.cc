#include "dc_content.h"

dc_content_local_t::dc_content_local_t(dc_api_ctx_default_server_info_t *server) : dc_content_t(server) {
    server_ = server;
}

dc_common_code_t
dc_content_local_t::do_file_attr(const std::string &path)
{

}

dc_common_code_t
dc_content_local_t::get_file_attr(dc_file_attr_t *attr)
{

}

dc_common_code_t
dc_content_local_t::do_file_content(const std::string &path)
{

}

dc_common_code_t
dc_content_local_t::get_file_content(std::vector<std::string> *content)
{

}