#include <csignal>
#include <unordered_set>

#include "dc_api_task.h"
#include "dc_common_trace_log.h"
#include "dc_compare.h"
#include "dc_internal_task.h"
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"
#include "workflow/WFFacilities.h"

using namespace wfrest;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo) { wait_group.done(); }

int main() {
    signal(SIGINT, sig_handler);

    HttpServer svr;
    dc_compare_t compare_worker;
    dc_internal_executor_t internal_executor;

    std::unordered_set<std::string> task_uuid_set;

    // 1. You can `./13_compess_client`
    // 2. or use python script `python3 13_compress_client.py`
    // 3.
    // echo '{"testgzip": "gzip compress data"}' | gzip | curl -v -i --data-binary @- -H "Content-Encoding: gzip"
    // http://localhost:8888/task echo '{"testgzip": "gzip compress data"}' | curl -v -i --data-binary @-
    // http://localhost:8888/task
    svr.POST("/task/{uuid}", [&compare_worker, &task_uuid_set](const HttpReq *req, HttpResp *resp) {
        // We automatically decompress the compressed data sent from the client
        // Support gzip, br only now
        // server: 这里设置压缩，注意，已经在header里面添加了相应的信息!
        resp->set_compress(Compress::GZIP);
        resp->add_header_pair("Content-Type", "application/json");

        const std::string &uuid = req->param("uuid");
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

        dc_api_task_t *task = new dc_api_task_t();
        // 这个task生命周期会在get的时候被销毁

        DC_COMMON_ASSERT(task != nullptr);

        auto ret = build_task_from_json(body.c_str(), body.size(), task);
        if (ret != S_SUCCESS) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "build_task_from_json failed: error:" + std::to_string((int)ret);
            resp->Json(json);
            return;
        }

        if (uuid != task->t_task_uuid) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "uuid is not equal to task->t_task_uuid";
            resp->Json(json);
            return;
        }

        // put this task into compare_worker
        ret = compare_worker.add(task);
        if (ret != S_SUCCESS) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "compare_worker.add failed: error:" + std::to_string((int)ret);
            resp->Json(json);
            return;
        }

        task_uuid_set.insert(uuid);

        // 这里设置返回的内容!
        Json json;
        json["uuid"] = uuid;
        json["msg"] = "put into queue, wait for schedule to run";
        resp->Json(json);
    });

    svr.GET("/internal/dir/task/{uuid}", [&internal_executor](const HttpReq *req, HttpResp *resp) {
        resp->set_compress(Compress::GZIP);
        resp->add_header_pair("Content-Type", "application/json");

        const std::string &uuid = req->param("uuid");
        if (uuid.empty()) {
            Json json;
            json["error"] = E_DC_INTERNAL_NO_UUID;
            resp->Json(json);
            return;
        }

        if (req->content_type() != APPLICATION_JSON) {
            Json json;
            json["error"] = E_DC_INTERNAL_NOT_JSON;
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        // 去查看一下uuid是否存在
        wfrest::Json resp_json_body;
        auto ret = internal_executor.get(uuid, resp_json_body);
        if (ret != S_SUCCESS) {
            Json json;
            json["error"] = ret;
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        resp->Json(resp_json_body);
    });

    svr.POST("/internal/dir/task/{uuid}", [&internal_executor](const HttpReq *req, HttpResp *resp) {
        // We automatically decompress the compressed data sent from the client
        // Support gzip, br only now
        // server: 这里设置压缩，注意，已经在header里面添加了相应的信息!
        resp->set_compress(Compress::GZIP);
        resp->add_header_pair("Content-Type", "application/json");

        const std::string &uuid = req->param("uuid");
        if (uuid.empty()) {
            Json json;
            json["error"] = E_DC_INTERNAL_NO_UUID;
            resp->Json(json);
            return;
        }

        if (req->content_type() != APPLICATION_JSON) {
            Json json;
            json["error"] = E_DC_INTERNAL_NOT_JSON;
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        auto &body = req->body();
        if (body.empty()) {
            Json json;
            json["error"] = E_DC_INTERNAL_EMPTY_BODY;
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        wfrest::Json json = wfrest::Json::parse(body);
        if (json.find("dirs") == json.end()) {
            Json json;
            json["error"] = E_DC_INTERNAL_NO_DIR_LIST;
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        if (json.find("uuid") == json.end()) {
            Json json;
            json["error"] = E_DC_INTERNAL_NO_UUID;
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        if (json["uuid"] != uuid) {
            Json json;
            json["error"] = E_DC_INTERNAL_UUID_NOT_EQUAL;
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        dc_internal_task_t task;
        task.i_uuid = uuid;
        task.i_task_type = DC_INTERNAL_TASK_TYPE_DIR_ATTR;
        task.i_json = json;

        // 返回值的格式
        // uuid: task_uuid
        // error: S_SUCCESS/other_code
        // result: [attr1, attr2]
        internal_executor.add(task);

        Json json_resp;
        json_resp["uuid"] = uuid;
        json_resp["error"] = S_SUCCESS;
        resp->Json(json_resp);
    });

    svr.GET("/task/{uuid}", [&compare_worker, &task_uuid_set](const HttpReq *req, HttpResp *resp) {
        const std::string &uuid = req->param("uuid");

        resp->set_compress(Compress::GZIP);
        resp->add_header_pair("Content-Type", "application/json");

        if (uuid.empty()) {
            Json json;
            json["error"] = "uuid is empty";
            json["uuid"] = uuid;
            resp->Json(json);
            return;
        }

        if (task_uuid_set.find(uuid) == task_uuid_set.end()) {
            Json json;
            json["uuid"] = uuid;
            json["error"] = "task not found";
            resp->Json(json);
            return;
        }

        Json result_json;
        auto ret = compare_worker.get(result_json);
        if (ret == S_SUCCESS) {
            DC_COMMON_ASSERT(ret == S_SUCCESS);
            resp->Json(result_json);
            task_uuid_set.erase(uuid);
            return;
        }

        if (ret == E_DC_TASK_MEM_VOPS_NOT_OVER) {
            result_json["uuid"] = uuid;
            result_json["errno"] = (int)ret;
            result_json["error"] = "task still running...";
            resp->Json(result_json);
            return;
        }

        task_uuid_set.erase(uuid);
        result_json["uuid"] = uuid;
        result_json["errno"] = (int)ret;
        result_json["error"] = "compare_worker.result failed: error:" + std::to_string((int)ret);
        resp->Json(result_json);
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
