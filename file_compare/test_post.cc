#include "workflow/WFTaskFactory.h"
#include "workflow/WFFacilities.h"
#include <signal.h>
#include "wfrest/Compress.h"
#include "wfrest/ErrorCode.h"

#include "workflow/WFFacilities.h"
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"

#include "chan.h"

using namespace protocol;
using namespace wfrest;

struct CompessContext
{
    std::string data;
};

static WFFacilities::WaitGroup wait_group(1);

std::string url = "http://127.0.0.1:8888";

msg_chan_t msg_q;

void http_callback(WFHttpTask *task)
{
    const void *body;
    size_t body_len;
    task->get_resp()->get_parsed_body(&body, &body_len);

    std::string decompress_data;
    int ret = Compressor::ungzip(static_cast<const char *>(body), body_len, &decompress_data);
    fprintf(stderr, "ret:%d Decompress Data : %s\n", ret, decompress_data.c_str());

    // use decompress_data to build a json
    wfrest::Json json = wfrest::Json::parse(decompress_data);
    if (json.find("errno") != json.end())
    {
        int err = json["errno"];
        std::string errmsg = json.find("error") != json.end() ? json["error"] : "";
        fprintf(stderr, "errno:%d, errmsg:%s\n", err, errmsg.c_str());
    }

    delete static_cast<CompessContext *>(task->user_data);

    msg_q.write(0);
}

int http_post_task(void)
{
    WFHttpTask *task = WFTaskFactory::create_http_task(url + "/task/b9460404-f559-4a93-be01-675045263713",
                                                       /*redirect_max*/4,
                                                       /*retry_max*/2,
                                                       http_callback);

    std::string request_body;
    // open input.json and read it to request_body
    std::string file_name = "input.json";
    FILE *fp = fopen(file_name.c_str(), "r");
    if (fp == NULL)
    {
        fprintf(stderr, "open file %s failed", file_name.c_str());
        return -1;
    }
    while (!feof(fp))
    {
        char buf[1024];
        size_t len = fread(buf, 1, sizeof(buf), fp);
        request_body.append(buf, len);
    }
    fclose(fp);

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

    return 0;
}

int main(int argc, char *argv[])
{
    http_post_task();
    msg_q.read_stuck();
    wait_group.done();
    wait_group.wait();
}
