#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"
#include <signal.h>
#include "wfrest/Compress.h"
#include "wfrest/ErrorCode.h"

using namespace protocol;
using namespace wfrest;

struct CompessContext
{
    std::string data;
};

static WFFacilities::WaitGroup wait_group(1);

void http_callback(WFHttpTask *task)
{
    const void *body;
    size_t body_len;
    task->get_resp()->get_parsed_body(&body, &body_len);

    std::string decompress_data;
    int ret = Compressor::ungzip(static_cast<const char *>(body), body_len, &decompress_data);
    fprintf(stderr, "ret:%d Decompress Data : %s", ret, decompress_data.c_str());
    delete static_cast<CompessContext *>(task->user_data);
    wait_group.done();
}

int main()
{
    std::string url = "http://127.0.0.1:8888";

    WFHttpTask *task = WFTaskFactory::create_http_task(url + "/task/b9460404-f559-4a93-be01-675045263713",
                                                       /*redirect_max*/4,
                                                       /*retry_max*/2,
                                                       http_callback);

    std::string request_body = R"(
    {
        "uuid": "b9460404-f559-4a93-be01-675045263713",
        "result_type": 0,
        "servers": [
            {
                "center": "m1",
                "host": "localhost",
                "user": "admin",
                "password": "admin",
                "port": 8091,
                "standard": true,
                "path_to_compare": "/tmp/res"
            },
            {
                "center": "m2",
                "host": "localhost",
                "user": "admin",
                "password": "admin",
                "port": 8091,
                "standard": false,
                "path_to_compare": "/tmp/ok"
            }
        ]
    }
    )";

    auto *ctx = new CompessContext;
    int ret = Compressor::gzip(&request_body, &ctx->data);
    if(ret != StatusOK)
    {
        ctx->data = std::move(request_body);
    }

    task->user_data = ctx;
    task->get_req()->set_method("POST");
    task->get_req()->add_header_pair("Content-Encoding", "gzip");
    task->get_req()->add_header_pair("Content-Type", "application/json");
    task->get_req()->append_output_body_nocopy(ctx->data.c_str(), ctx->data.size());
    task->start();
    wait_group.wait();
}
