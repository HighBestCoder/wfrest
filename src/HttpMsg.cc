#include "HttpMsg.h"
#include "workflow/HttpUtil.h"
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h>
#include <spdlog/spdlog.h>
#include "HttpTaskUtil.h"
#include "UriUtil.h"
#include "StrUtil.h"

using namespace wfrest;

std::string HttpReq::Body() const
{
    return protocol::HttpUtil::decode_chunked_body(this);
}

std::string HttpReq::default_query(const std::string &key, const std::string &default_val)
{
    if (query_params_.count(key))
        return query_params_[key];
    else
        return default_val;
}

bool HttpReq::has_query(const std::string &key)
{
    if (query_params_.find(key) != query_params_.end())
    {
        return true;
    } else
    {
        return false;
    }
}

void HttpReq::parse_body()
{
    const void *body;
    size_t len;
    this->get_parsed_body(&body, &len);
    StringPiece body_str(body, len);
    fprintf(stderr, "body size : %zu\n", body_str.size());
    if(body_str.empty()) return;

    fill_content_type();

    switch (content_type)
    {
        case X_WWW_FORM_URLENCODED:
            Urlencode::parse_query_params(body_str, kv);
            break;
        case MULTIPART_FORM_DATA:
            multi_part_.parse_multipart(body_str, form);
            break; // do nothing
        default:
            break;// do nothing
    }
}

void HttpReq::fill_content_type()
{
    std::string content_type_str = header("Content-Type");
    fprintf(stderr, "content : %s\n", content_type_str.c_str());
    content_type = ContentType::to_enum(content_type_str);

//    if (content_type == CONTENT_TYPE_NONE) {
//        if (!form.empty()) {
//            content_type = MULTIPART_FORM_DATA;
//        }
//        else if (!kv.empty()) {
//            content_type = X_WWW_FORM_URLENCODED;
//        }
//        else {
//            content_type = TEXT_PLAIN;
//        }
//    }
    // todo : we need fill this in header? add interface to change the header value?
    // if (content_type != CONTENT_TYPE_NONE) {
    //    header("Content-Type") = ContentType::to_string(content_type);
    // }

    if (content_type == MULTIPART_FORM_DATA)
    {
        // if type is multipart form, we reserve the boudary first
        const char *boundary = strstr(content_type_str.c_str(), "boundary=");
        if (boundary == nullptr)
        {
            // todo : do we need to add default to header field ?
            // header("Content-Type") += "; boundary=" + MultiPartForm::default_boundary;
            // multi_part_.set_boundary(MultiPartForm::default_boundary);
            return;
        }
        boundary += strlen("boundary=");
        StringPiece boundary_piece(boundary);

        StringPiece boundary_str = StrUtil::trim_pairs(boundary_piece, R"(""'')");
        // todo :
        fprintf(stderr, "boundary : %s\n", boundary_str.data());
        fprintf(stderr, "boundary : %s\n", boundary_str.as_string().c_str());

        multi_part_.set_boundary(std::move(boundary_str.as_string()));
    }
}

void HttpResp::String(const std::string &str)
{
    // bool append_output_body(const void *buf, size_t size);
    this->append_output_body(static_cast<const void *>(str.c_str()), str.size());
}

void HttpResp::String(std::string &&str)
{
    this->append_output_body(static_cast<const void *>(str.c_str()), str.size());
}

void HttpResp::String(const char *data, size_t len)
{
    this->append_output_body(static_cast<const void *>(data), len);
}


/*
We do not occupy any thread to read the file, but generate an asynchronous file reading task
and reply to the request after the reading is completed.

We need to read the whole data into the memory before we start replying to the message. 
Therefore, it is not suitable for transferring files that are too large.

todo : Any better way to transfer large File?
*/
void pread_callback(WFFileIOTask *pread_task)
{
    FileIOArgs *args = pread_task->get_args();
    long ret = pread_task->get_retval();
    auto *resp = static_cast<HttpResp *>(pread_task->user_data);
    close(args->fd);
    if (pread_task->get_state() != WFT_STATE_SUCCESS || ret < 0)
    {
        resp->set_status_code("503");
        resp->append_output_body("<html>503 Internal Server Error.</html>");
    } else
    {
        resp->append_output_body_nocopy(args->buf, ret);
    }
}

void HttpResp::File(const std::string &path)
{
    auto *server_task = task_of(this);
    assert(server_task);

    int fd = open(path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        size_t size = lseek(fd, 0, SEEK_END);
        void *buf = malloc(size);
        WFFileIOTask *pread_task = WFTaskFactory::create_pread_task(fd, buf,
                                                                    size, 0,
                                                                    pread_callback);
        server_task->user_data = buf; /* to free() in callback() */
        pread_task->user_data = this;   /* pass resp pointer to pread task. */
        server_task->set_callback([](WebTask *server_task)
                                  {
                                      free(server_task->user_data);
                                  });
        **server_task << pread_task;
    } else
    {
        set_status_code("404");
        append_output_body("<html>404 Not Found.</html>");
    }
}

void HttpResp::set_status(int status_code)
{
    protocol::HttpUtil::set_response_status(this, status_code);
}



