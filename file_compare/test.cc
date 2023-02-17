#include "dc_api.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>                      /* errno            */
#include <time.h>                       /* nanosleep        */

#ifndef PATH_MAX
#define PATH_MAX                        1024
#endif

#define RESULT_BUF                      16777216

static char                             task_json_str[1048576];                 /* 目前task.json最大1MB*/

#ifdef WRITE_TO_FILE
static char                             task_result[RESULT_BUF];                  /* 目前最多16MB        */
#endif

static int                              task_result_bytes;

static char                             task_uuid[PATH_MAX];

static void
main_exit_with_help(void)
{
    printf(
        "Usage: ./test task.json\n"
        "Example:\n\n"
        "1. Just one task\n"
        "./test task1.json\n"
    );
    exit(-1);
}

static void
main_send_task(int compare_idx,
               const char *task_json_file)
{
    FILE                                *fp;
    int                                 error;
    cJSON                               *root;
    cJSON                               *item;
    int                                 read_bytes;

    fp = fopen(task_json_file, "r");
    if (fp == NULL) {
        printf("open file %s failed\n", task_json_file);
        exit(-1);
    }

    memset(task_json_str, 0, sizeof(task_json_str));
    read_bytes = fread(task_json_str, 1, sizeof(task_json_str), fp);
    if (read_bytes < 0) {
        printf("read_bytes = %d failed\n", read_bytes);
        exit(-1);
    }
    fclose(fp); /* 读取task.json结束      */

    root = NULL;
    root = cJSON_Parse(task_json_str);
    if (root == NULL) {
        printf("parse json file:%s failed\n", task_json_file);
        exit(-1);
    }

    item = NULL;
    item = cJSON_GetObjectItem(root, "uuid");
    if (item == NULL) {
        printf("get uuid item failed\n");
        exit(-1);
    }

    printf("[INFO] task_uuid:%s\n", item->valuestring);
    memset(task_uuid, 0, sizeof(task_uuid));
    memcpy(task_uuid, item->valuestring, strnlen(item->valuestring, PATH_MAX));

    cJSON_Delete(root);
    root = NULL;

    error = dc_api_task_add(compare_idx,
                            task_json_str,
                            strnlen(task_json_str, PATH_MAX),
                            0 /* JSON*/);
    if (error) {
        printf("dc_api_task_add error no %d\n", error);
        exit(-1);
    }

    printf("[INFO] add task:%s into dc_api\n", task_uuid);
}

#ifdef WRITE_TO_FILE
static void
main_write_result_to_file(const char *fn,
                          const char *buf,
                          const uint32_t buf_len)
{
    FILE                                *fp;
    int                                 wbytes;

    fp = NULL;
    fp = fopen(fn, "w");
    if (fp == NULL) {
        printf("open file %s failed\n", fn);
        exit(-1);
    }

    wbytes = fwrite(buf, 1, buf_len, fp);
    if (wbytes != (int)buf_len) {
        printf("write file:%s failed, ret = %d, errno:%d\n", fn, wbytes, errno);
        exit(-1);
    }
    fclose(fp); /* 读取task.json结束      */
}
#endif

int
main(int argc, char **argv)
{
    dc_api_ctx_idx_t                    compare_idx;
    const char                          *config_file_path;
    uint32_t                            config_file_path_len;
    int                                 error;

    if (argc != 2) {
        main_exit_with_help();
        return -1;
    }

    error = dc_api_ctx_construct(&compare_idx);
    if (error) {
        printf("dc_api_ctx_construct error no %d\n", error);
        return -1;
    }

    main_send_task(compare_idx, argv[1]);

    error = dc_api_task_start(compare_idx,
                              task_uuid,
                              strnlen(task_uuid, PATH_MAX));
    if (error) {
        printf("dc_api_task_start error no %d\n", error);
        return -1;
    }

    task_result_bytes = 0;
    while (1) {
        struct timespec                 req;

        req.tv_sec = 0;
        req.tv_nsec = 100;

        nanosleep(&req, NULL);

        task_result_bytes--;
        error = dc_api_task_get_result(compare_idx,
                                       task_uuid,
                                       strnlen(task_uuid, PATH_MAX),
                                       0 /*JSON*/,
#ifdef WRITE_TO_FILE
                                       task_result,
                                       RESULT_BUF,
#else
                                        NULL,
                                        0,
#endif
                                       &task_result_bytes);
        /*
         * 因为是异步的，所以有可能任务还在执行
         * 并未结束
         */
        if (error == 18 /*还没有结束 */) {
            /* 最好是在这里有个短暂的sleep*/
            continue;
        }

        if (error) {
            printf("dc_api_task_get_result error no %d\n", error);
            return -1;
        }

        printf("[DEBUG] ret = %d, task_result_bytes = %d\n", error, task_result_bytes);

        break; /* 获取任务执行结果成功 */
    }

    /*
    error = dc_api_task_cancel(compare_idx,
                               task_uuid,
                               strnlen(task_uuid, PATH_MAX));
    if (error) {
        printf("dc_api_ctx_task_cancel error no %d\n", error);
        return -1;
    }*/

    error = dc_api_ctx_destroy(&compare_idx);
    if (error) {
        printf("dc_api_ctx_destroy error no %d\n", error);
        return -1;
    }

    printf("[DEBUG] over at %s(%s:%d)\n", __FUNCTION__, __FILE__, __LINE__);

    return 0;
}
