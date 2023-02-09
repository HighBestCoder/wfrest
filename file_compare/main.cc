#include "workflow/WFFacilities.h"
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"

#include "dc_api.h"

#include <csignal>

using namespace wfrest;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    dc_api_ctx_idx_t ctx_idx;
    dc_common_code_t ret;

    ret = dc_api_ctx_construct(&ctx_idx);
    if (ret != S_SUCCESS) {
        fprintf(stderr, "dc_api_ctx_construct failed\n");
        return -1;
    }

    // use shared ptr as defer to release ctx
    std::shared_ptr<void> defer(nullptr, [&ctx_idx](void *) {
        dc_api_ctx_destroy(&ctx_idx);
    });

    signal(SIGINT, sig_handler);

    HttpServer svr;

    // 1. You can `./13_compess_client` 
    // 2. or use python script `python3 13_compress_client.py`
    // 3.
    // echo '{"testgzip": "gzip compress data"}' | gzip | curl -v -i --data-binary @- -H "Content-Encoding: gzip" http://localhost:8888/task
    // echo '{"testgzip": "gzip compress data"}' | curl -v -i --data-binary @- http://localhost:8888/task
    svr.POST("/task/{uuid}", [&ctx_idx](const HttpReq *req, HttpResp *resp) {
        // We automatically decompress the compressed data sent from the client
        // Support gzip, br only now
        const std::string& uuid = req->param("uuid");
        if (uuid.empty()) {
            resp->String("uuid is empty");
            return;
        }

        if (req->content_type() != APPLICATION_JSON) {
            resp->String(uuid + std::string("NOT APPLICATION_JSON"));
            return;
        }

        auto &body = req->body();

        // server: 这里设置压缩，注意，已经在header里面添加了相应的信息!
        resp->set_compress(Compress::GZIP);

        dc_common_code_t ret = dc_api_task_add(ctx_idx,
                                               body.data(),
                                               body.size(),
                                               DC_CONFIG_TYPE_JSON);
        if (ret != S_SUCCESS) {
            resp->String("{\"uuid\": \"" + uuid + "\"}");
            return;
        }

        // 这里设置返回的内容!
        resp->String("Test for server send gzip data\n");
    });

    if (svr.start(8888) == 0) {
        wait_group.wait();
        svr.stop();
    } else {
        fprintf(stderr, "Cannot start server");
        exit(1);
    }

    return 0;
}
