#ifndef _DC_DIFF_H_
#define _DC_DIFF_H_

#include "dc_api_task.h"
#include "dc_common_error.h"
#include "chan.h"

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <list>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <stdio.h>
#include <list>
#include <map>

class dc_diff_t {
  public:
    virtual ~dc_diff_t() {}
  
    virtual dc_common_code_t build_filter_rows(dc_api_task_t *task) = 0;
    virtual dc_common_code_t add_row(const int idx,
                                     const uint32_t cols_nr,
                                     const uint64_t *length,
                                     const uint8_t **cols) = 0;
    virtual dc_common_code_t handle_failure(const int idx) = 0;
    virtual dc_common_code_t gen_diff(dc_api_task_t *task) = 0;
  protected:
    void write_json_header(FILE *fp, dc_api_task_t *task);
    void write_json_tail(FILE *fp, dc_api_task_t *task);
};

struct dc_diff_single_row_t {
    void build(const uint32_t cols_nr,
               const uint64_t *length,
               const uint8_t **cols);

    void build(const dc_diff_single_row_t *other,
               const uint32_t cols_nr,
               const uint64_t *length,
               const uint8_t **cols);

    bool is_same(const uint32_t cols_nr,
                 const uint64_t *length,
                 const uint8_t **cols) const;

    std::string to_string(void) const;

    uint32_t r_cols_nr { 0 };
    uint64_t *r_length { nullptr };
    uint8_t **r_cols { nullptr };

    uint8_t *mem_addr { nullptr };
};

// 这里是行的集合。
// 但是从逻辑上来说，这里存放的是所有db里面pk一样的行
// 所以也只能算是一行!
struct dc_diff_row_t {
    int find_same_row(const uint32_t cols_nr,
                      const uint64_t *length,
                      const uint8_t **cols);
    dc_common_code_t add_row(const int idx,
                             const uint32_t cols_nr,
                             const uint64_t *length,
                             const uint8_t **cols);

    // 是不是所有的data center都已经把这一行的数据都加进来了
    // 除了出错的以外!
    bool has_full_with_rows(void) const;

    // 查看是否需要发送到diff generator
    bool all_rows_are_same(void) const;

    dc_diff_row_t(const int sn) { r_idxs.resize(sn, -1); }
    ~dc_diff_row_t() {
      for (auto r: r_rows) {
        free(r->mem_addr);
        delete r;
      }
    }

    // 这里是用来存放不同的行
    std::vector<dc_diff_single_row_t*> r_rows;
    // 这里存放每个server_info对应的行的下标
    std::vector<int> r_idxs;
    // 指向对应的task
    dc_api_task_t *r_task { nullptr};
    // 是否是一个退出任务
    bool r_exit_tag { false};
};

class dc_diff_hash_t : public dc_diff_t {
  public:
    dc_diff_hash_t(dc_api_task_t *task);
    virtual ~dc_diff_hash_t();

    virtual dc_common_code_t add_row(const int idx,
                                     const uint32_t cols_nr,
                                     const uint64_t *length,
                                     const uint8_t **cols) override;

    virtual dc_common_code_t handle_failure(const int idx) override;

    // 当compare认为所有的行都加进来之后，就会调用这个函数，生成diff
    virtual dc_common_code_t gen_diff(dc_api_task_t *task) override;

  protected: // 需要子类实现
    virtual dc_common_code_t do_add_to_hash(const int idx,
                                       const uint32_t cols_nr,
                                       const uint64_t *length,
                                       const uint8_t **cols) = 0;
    virtual dc_common_code_t do_handle_failure(const int idx) = 0;
    virtual void do_flush_hash_map(dc_api_task_t *task) = 0;

  protected: // 子类公共helpers
    dc_api_task_t *task_;
    void send_diff(dc_diff_row_t *diff);     // 主线程发现有差异可以生成，发送过去!

  private: // helpers
    std::mutex write_down_list_mutex_;
    std::condition_variable write_down_list_cond_;
    std::list<dc_diff_row_t*> write_down_list_;

    void thd_write_down(); // 线程函数

    dc_diff_row_t *pop_diff(void);      // 从线程获取差异，写入文件

    void write_down1(FILE *fp, dc_diff_row_t *diff);        // 写入文件
    void write_down2(FILE *fp, dc_diff_row_t *diff);        // 写入文件
    void write_down(FILE *fp, dc_diff_row_t *diff);         // 写入文件

  private:
    void wait_write_content_over();
    void notify_write_content_over();

  private:
    const uint8_t *convert_to_json_str(const uint8_t *src,
                                       uint64_t src_len,
                                       uint64_t *dst_len);

  private:
    std::thread thd_write_down_;

    std::atomic<bool> thd_write_down_exit_{false};

    std::mutex thd_content_write_over_mutex_;
    std::condition_variable thd_content_write_over_cond_;
    bool thd_content_write_over_ {false};

    uint8_t *json_buf_ { nullptr };
    uint32_t json_buf_len_ { 0 };
};

class dc_diff_string_hash_t : public dc_diff_hash_t {
  public:
    dc_diff_string_hash_t(dc_api_task_t *task);
    virtual ~dc_diff_string_hash_t();

    virtual dc_common_code_t build_filter_rows(dc_api_task_t *task) override;

  protected:
    virtual dc_common_code_t do_add_to_hash(const int idx,
                                       const uint32_t cols_nr,
                                       const uint64_t *length,
                                       const uint8_t **cols) override;

    virtual dc_common_code_t do_handle_failure(const int idx) override;

    virtual void do_flush_hash_map(dc_api_task_t *task) override;
  private:
    std::unordered_map<std::string/*主键构成的hash字符串*/, dc_diff_row_t*> diff_map_;
    char *hash_key_buf_ { nullptr };
    uint32_t hash_key_buf_len_ { 0 };

    std::unordered_set<std::string> filter_set_;
};

class dc_diff_int_hash_t : public dc_diff_hash_t {
  public:
    dc_diff_int_hash_t(dc_api_task_t *task) : dc_diff_hash_t(task) {}
    virtual ~dc_diff_int_hash_t() {}

    virtual dc_common_code_t build_filter_rows(dc_api_task_t *task) override;

  protected:
    virtual dc_common_code_t do_add_to_hash(const int idx,
                                       const uint32_t cols_nr,
                                       const uint64_t *length,
                                       const uint8_t **cols) override;

    virtual dc_common_code_t do_handle_failure(const int idx) override;

    virtual void do_flush_hash_map(dc_api_task_t *task) override;
  private:
    std::unordered_map<uint64_t, dc_diff_row_t*> diff_map_;
    std::unordered_set<uint64_t> filter_set_;
};

#endif /* _DC_DIFF_H_ */