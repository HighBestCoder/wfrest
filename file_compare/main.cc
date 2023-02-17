#include "workflow/WFFacilities.h"
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"

#include "dc_api.h"
#include "dc_common_trace_log.h"

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
        // server: 这里设置压缩，注意，已经在header里面添加了相应的信息!
        resp->set_compress(Compress::GZIP);
        resp->add_header_pair("Content-Type", "application/json");

        const std::string& uuid = req->param("uuid");
        if (uuid.empty()) {
            Json json;
            json["error"] = "uuid is empty";
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        if (req->content_type() != APPLICATION_JSON) {
            Json json;
            json["error"] = "content type is not json";
            json["content_type"] = req->content_type();
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        auto &body = req->body();
        if (body.empty()) {
            Json json;
            json["error"] = "body is empty";
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        LOG(DC_COMMON_LOG_INFO, "task:%s body:%s", uuid.c_str(), body.c_str());

        dc_common_code_t ret = dc_api_task_add(ctx_idx,
                                               body.data(),
                                               body.size(),
                                               DC_CONFIG_TYPE_JSON);
        if (ret != S_SUCCESS) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "dc_api_task_add failed: error:" + std::to_string((int)ret);
            resp->Json(json);
            return;
        }

        // try to start the task
        ret = dc_api_task_start(ctx_idx, uuid.c_str(), uuid.size());
        if (ret != S_SUCCESS) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "dc_api_task_start failed: error:" + std::to_string((int)ret);
            resp->Json(json);
            return;
        }

        // 这里设置返回的内容!
        Json json;
        json["uuid"] = uuid;
        resp->Json(json);
    });

    svr.GET("/task/{uuid}", [&ctx_idx](const HttpReq *req, HttpResp *resp) {
        const std::string& uuid = req->param("uuid");

        resp->set_compress(Compress::GZIP);
        resp->add_header_pair("Content-Type", "application/json");

        if (uuid.empty()) {
            Json json;
            json["error"] = "uuid is empty";
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        int need_bytes = 0;
        dc_common_code_t ret = dc_api_task_get_result(ctx_idx,
                                                      uuid.c_str(),
                                                      uuid.size(),
                                                      DC_CONFIG_TYPE_JSON,
                                                      0,
                                                      0,
                                                      &need_bytes);
        if (ret == E_DC_CONTENT_RETRY || ret == E_DC_TASK_MEM_VOPS_NOT_OVER) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "retry";
            resp->Json(json);
            return;
        }

        if (ret != S_SUCCESS) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "dc_api_task_get_result failed: error:" + std::to_string((int)ret);
            resp->Json(json);
            return;
        }

        std::string result;
        result.resize(need_bytes, 0);
        ret = dc_api_task_get_result(ctx_idx,
                                     uuid.c_str(),
                                     uuid.size(),
                                     DC_CONFIG_TYPE_JSON,
                                     (char*)result.data(),
                                     need_bytes,
                                     &need_bytes);
        if (ret != S_SUCCESS) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "dc_api_task_get_result2 failed: error:" + std::to_string((int)ret);
            resp->Json(json);
            return;
        }

        LOG(DC_COMMON_LOG_INFO, "task:%s result:%s", uuid.c_str(), result.c_str());

        Json json(result);
        resp->Json(json);
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
