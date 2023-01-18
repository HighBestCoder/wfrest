#include "workflow/WFFacilities.h"
#include <csignal>
#include "wfrest/HttpServer.h"
#include "wfrest/json.hpp"

using namespace wfrest;
using Json = nlohmann::json;

static WFFacilities::WaitGroup wait_group(1);

void sig_handler(int signo)
{
    wait_group.done();
}

int main()
{
    signal(SIGINT, sig_handler);

    HttpServer svr;
    
    // 1. You can `./13_compess_client` 
    // 2. or use python script `python3 13_compress_client.py`
    // 3.
    // echo '{"testgzip": "gzip compress data"}' | gzip | curl -v -i --data-binary @- -H "Content-Encoding: gzip" http://localhost:8888/task
    // echo '{"testgzip": "gzip compress data"}' | curl -v -i --data-binary @- http://localhost:8888/task
    svr.POST("/task", [](const HttpReq *req, HttpResp *resp) {
        // We automatically decompress the compressed data sent from the client
        // Support gzip, br only now

        if (req->content_type() != APPLICATION_JSON) {
            resp->String("NOT APPLICATION_JSON");
            return;
        }

        auto &body_json = req->json();
        /*
         body_json example:

         {
            "id": "0b4e",
            "basis_server": {
                "id": "1abb",
                "name": "drp0",
                "ip": "127.0.0.1",
                "path": "/home/a/xxx"
            },
            "compare_server_list": [
                {
                    "id": "1abb",
                    "name": "drp1",
                    "ip": "127.0.0.1",
                    "path": "/home/a/xxx"
                }],
            "file_store_path": "/nfs/file"
            },
         */



        // server: 这里设置压缩，注意，已经在header里面添加了相应的信息!
        resp->set_compress(Compress::GZIP);
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
