#ifndef _DC_FAILED_CONTENT_H_
#define _DC_FAILED_CONTENT_H_

#include "dc_api_task.h"
#include "dc_common_error.h"
#include "dc_diff.h"
#include "dc_common_assert.h"

class dc_diff_failed_content_t : public dc_diff_t {
    public:
        dc_diff_failed_content_t(dc_api_task_t * task) : task_(task) {}
        virtual ~dc_diff_failed_content_t() {}
    
        virtual dc_common_code_t build_filter_rows(dc_api_task_t *task) override {
            return S_SUCCESS;
        }

        virtual dc_common_code_t add_row(const int idx,
                                         const uint32_t cols_nr,
                                         const uint64_t *length,
                                         const uint8_t **cols) override {
            return S_SUCCESS;
        }

        virtual dc_common_code_t handle_failure(const int idx) override {
            return S_SUCCESS;
        }

        virtual dc_common_code_t gen_diff(dc_api_task_t *task) override {
            dc_common_code_t ret = S_SUCCESS;

            // generate header of json
            FILE *fp = NULL;
            fp = fopen(task_->t_task_uuid.c_str(), "w");
            DC_COMMON_ASSERT(fp != NULL);

            write_json_header(fp, task);
            write_json_tail(fp, task);

            fclose(fp);
            fp = NULL;

            return ret;
        }
    private:
        dc_api_task_t *task_ { nullptr };
};

#endif /* ! _DC_FAILED_CONTENT_H_ */