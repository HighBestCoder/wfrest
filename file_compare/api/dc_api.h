#ifndef _DC_API_H_
#define _DC_API_H_

#include "dc_common_error.h" /* code_t           */

#include <stdint.h> /* uint/intxx_t     */

#ifdef __cplusplus
extern "C"
{
#endif

    typedef int dc_api_ctx_idx_t;

    /*
     * TODO: 返回结果的json的格式
     * - 1. 版本信息
     * - 2. 出错信息
     * - 3. 日志
     *
     * TODO:
     * - 部署的结构，是否可以一个比较服务跨多个数据中心访问。
     * - 接口的使用示范. (p0)
     * - 文件比较的需求需要确定！
     *
     */

    /*
     * NOTE:
     *
     * 传进来的常量指针，在C语言里面不能保存一个指针
     * 指向这些字符串。
     *
     * 如果需要保存这些字符串，那么需要将这些字符串进行copy
     *
     */

    /*
     * 从逻辑上来说，可以生成`不同的比较服务`
     * 每个比较服务负责管理不同的数据比较服务。
     *
     * 比如:
     *
     * - dc_api_ctx_idx_t = 0的服务可以负责客户1的比较服务
     * - dc_api_ctx_idx_t = 1的服务可以负责客户2的比较服务
     *
     * 或者
     * - dc_api_ctx_idx_t = 0表示代码v0的比较服务。
     * - dc_api_ctx_idx_t = 1表示代码v1的比较服务。
     *
     * 而这两个客户的数据比较本身是不相关的. 如果不想区分客户
     * 那么就只需要创建一个，然后idx_t=0，一直使用0就可以了。
     *
     * 参数:
     * - config_file_path   : 生成比较服务的配置
     *                            这个配置里面可以记录比较服务的版本
     *                            比如 {"version": 1}
     *                            这样可以实现同时加载不同版本的比较服务
     *                            都可以运行!
     *                            TODO: 文件内容与格式待定!
     * - config_file_path_len: 路径长度
     * - config_type        : config的格式，默认是json
     * - ctx                : 一个指向int的指针，用来存放得到的比较服务的索引
     *
     * 返回值：
     * - 0                  : 成功
     * - other              : 其他各种值表示各种失败含义
     *
     * hint: 可以指定日志的路径!
     */
    dc_common_code_t
    dc_api_ctx_construct(dc_api_ctx_idx_t *ctx /*OUT*/);      /* 比较服务的索引        */

    dc_common_code_t
    dc_api_ctx_perf(dc_api_ctx_idx_t ctx,
                    const int perf_out_type,
                    char *perf_out_buf,
                    char *perf_out_buf_len,
                    int *result_bytes);

    dc_common_code_t
    dc_api_ctx_destroy(dc_api_ctx_idx_t *ctx /*CHANGED*/); /* 销毁一个比较服务      */

    /*
     * 添加一个需要管理的客户端到比较服务上
     * 客户端的例子，比如:
     *
     * - DB2 Connection
     * - MySQL Connection
     * - FileSystem比较Agent的connection
     * - 链接管理服务: 各种认证信息需要从这里申请
     *
     * 通过将这些链接添加到auth部分之后，后续在比较的时候
     * 可以通过指定auth_name来决定一个比较任务应该如何去
     * 获得认证信息!
     *
     * HINT:
     * - auth_name需要唯一!
     * - 如果要构建的auth_name已经存在，则更新之!
     *
     * 参数:
     *
     * - ctx                : 构建的比较服务器的索引
     * - auth_info          : 比较服务器要需要的链接信息
     * - auth_info_len      : auth_info字符串的长度
     * - auth_type          : auth_info的格式，比如json/yaml等等
     *                        这里我们用int来表示。比如：
     *                      -o 0表示json (default) (TODO:具体内容待定!)
     *                         这里只是举个例子!
     *                         - * {
     *                                      "name":     user_name,
     *                                      "password": user_password,
     *                                      "host":     db_host,
     *                                      "forward":  true/false,
     *                             }
     *                      -o 1表示yaml
     *                      -o 2表示ini
     *
     * 返回值：
     * - 0                  : 成功
     * - other              : 其他各种值表示各种失败含义
     *
     */
    dc_common_code_t
    dc_api_auth_add(dc_api_ctx_idx_t ctx,         /* 比较服务的索引       */
                    const char *auth_info,        /* 认证信息            */
                    const uint32_t auth_info_len, /* 认证信息字符串长度    */
                    const int auth_type);         /* 认证信息的格式       */

    /*
     * 查询数据库的认证信息
     *
     * 可以通过auth_name，得到相应的认证信息：
     * 比如，查询mysql/db2的链接信息.
     *
     * 如果认证信息是交由第三方管理，那么这里将会去
     * 第三方请求认证信息，并且将认证信息返回!
     *
     * HINT:
     * - auth_name需要唯一!
     * - 如果要构建的auth_name已经存在，则更新之!
     *
     * 参数：
     * - ctx                : 比较服务的索引
     * - auth_name          : 通过auth_name查询认证信息
     * - auth_name_len      : auth name字符串的长度
     * - out_buf            : 将查询到的结果放到out_buf
     * - out_buf_len        : out_buf的缓冲区长度
     * - auth_info_len      : auth_info占的bytes数。
     * - auth_out_type      : auth信息输出的格式, json/yaml/ini
     *
     * 返回值：
     * - 0                  : 成功
     * - other              : 其他各种值表示各种失败含义
     *
     */
    dc_common_code_t
    dc_api_auth_get(dc_api_ctx_idx_t ctx,         /* 比较服务的索引       */
                    const char *auth_name,        /* 认证信息的索引       */
                    const uint32_t auth_name_len, /* 认证信息的长度       */
                    const int auth_out_type,      /* 输出auth_info的格式 */
                    char *out_buf,                /* 输出要查询的认证信息  */
                    const uint32_t out_buf_len,   /* 输出缓冲区的长度     */
                    int *auth_info_bytes /*OUT*/);

    /*
     * 删除数据库的认证信息
     *
     * 可以通过auth_name，删除相应的认证信息：
     * 比如，如果有mysql/db2的认证信息. 将这部分
     * 信息删除!
     *
     * 参数：
     * - ctx                : 比较服务的索引
     * - auth_name          : 通过auth_name查询认证信息
     * - auth_name_len      : auth name字符串的长度
     *
     * 返回值：
     * - 0                  : 成功
     * - other              : 其他各种值表示各种失败含义
     */
    dc_common_code_t
    dc_api_auth_delete(dc_api_ctx_idx_t ctx,          /* 比较服务的索引     */
                       const char *auth_name,         /* 认证信息的索引     */
                       const uint32_t auth_name_len); /* 认证信息name长度   */

    /*
     * 添加任务
     *
     * !注意! 此时，并不会立马触发任务。
     * 这里只是把任务添加到系统中。
     *
     * !注意! 是添加任务，不是定义任务。
     * 如果同一种服务，需要跑两次，那么需要给不同的uuid
     *
     * !注意!后续在查询任务状态/结果的时候，需要用到任务的uuid.
     *
     * 参数:
     * - ctx                : 比较服务的索引
     * - task_content       : 比较服务的内容，以json为例(!待定!)
     *                      - {
     *                              "uuid":           "唯一的UUID",
     *                              "auth_name":    "认证信息",
     *                              "sql":          "需要执行的sql",
     *                        }
     *
     * - task_content       : 任务字符串的长度
     * - task_content_type  : json/yaml/ini (检查前面的enum)
     *
     * 返回值:
     * - 0                  : 表示成功
     * - other              : 表示失败!
     */
    dc_common_code_t
    dc_api_task_add(dc_api_ctx_idx_t ctx,            /* 比较服务的索引    */
                    const char *task_content,        /* 任务的定义       */
                    const uint32_t task_content_len, /* 任务字符串的长度  */
                    const int task_content_type);    /* 任务定义的格式    */

    /*
     * 启动任务
     *
     * 启动定义好的任务。
     *
     *
     * 参数：
     * - ctx                : 比较服务的索引
     * - task_uuid          : 服务的uuid
     * - task_uuid          : uuid字符串的长度
     *
     * 返回值
     * - 0                  : 启动任务成功
     * - other              : 启动任务失败
     */
    dc_common_code_t
    dc_api_task_start(dc_api_ctx_idx_t ctx,          /* 比较服务的索引    */
                      const char *task_uuid,         /* 任务的索引       */
                      const uint32_t task_uuid_len); /* 任务索引的长度    */

    /*
     * 获取任务的结果
     *
     * 参数：
     * - ctx                : 比较服务的索引
     * - task_uuid          : task的uuid
     * - task_uuid_len      : task_uuid字符的长度
     *
     * - task_out_buf_len   : 输出任务的结果
     * - result_bytes       : 拿走任务结果需要的内存(bytes)!
     *
     * !注意!
     * - task_out_buf_len 要足够长才可以拷走任务的结果
     *   任务的内容长度会返回在result_buf_len里面。
     *
     * - 如果task_out_buf_len长度不足，那么此时什么也不做
     *   只是把任务的内容放到result_buf_len中。
     *
     * - 所以，在这里,task_out可以为空。task_out_buf长度可以为0
     *
     * 返回值
     * - 0                  : 启动任务成功
     * - other              : 启动任务失败
     */
    dc_common_code_t
    dc_api_task_get_result(dc_api_ctx_idx_t ctx,                        /* 比较服务的索引    */
                           const char *task_uuid,                       /* 任务的索引       */
                           const uint32_t task_uuid_len,                /* 任务索引的长度    */
                           const int out_format,                        /* 输出的格式json...*/
                           char *task_out_buf, /*nullable*/             /* 任务结果缓冲区    */
                           const uint32_t task_out_buf_len /*maybe 0*/, /* 任务缓冲区长度    */
                           int *result_bytes /*OUT*/);                  /* 任务结果bytes数   */

    /*
     * 取消任务
     *
     * 参数：
     * - ctx                : 比较服务的索引
     * - task_uuid          : 服务的uuid
     *
     * !注意!
     * 任务的取消分为两种情况。
     * 1. 任务还没有运行。那么后面将不能再调用start/get_result来执行
     * 2. a. 如果任务已经运行，并且是没有分阶段，那么cancel将没有任何作用。
     *       任务还是会一直运行，直到结束。但是，执行的结果会被扔掉。
     *    b. 如果任务已经运行，并且是分阶段的，那么将在当前的分片结束之后
     *       任务将结束。并且，扔掉执行的结果。后面的分片将不再运行。
     *
     * 返回值
     * - 0                  : 启动任务成功
     * - other              : 启动任务失败
     */
    dc_common_code_t
    dc_api_task_cancel(dc_api_ctx_idx_t ctx,
                       const char *task_uuid,
                       const uint32_t task_uuid_len);

#ifdef __cplusplus
}
#endif

#endif /*! _DC_API_H_ */
