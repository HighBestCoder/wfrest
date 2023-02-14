#ifndef EX_CHAN_H_
#define EX_CHAN_H_

#include <atomic>
#include <stdexcept>

template <typename T>
class SPSCQueue {
   public:
    explicit SPSCQueue(const int cap) noexcept : cap_(std::max<int>(cap + 1, 4)) {
        buf_ = (T*)(operator new[](sizeof(T) * cap_));
    }

    ~SPSCQueue() {
        while (front()) {
            pop();
        }
        operator delete[](buf_);
    }

    template <typename... Args>
    bool push(Args &&... args) noexcept {
        auto const head = head_.load(std::memory_order_relaxed);
        auto next_head = head + 1;
        if (next_head == cap_) {
            next_head = 0;
        }

        // 如果下一个要放的内容，还没有被取走
        // head是要存放的空间，这个空间必须要是空的。
        // 这里采用的是循环队列是[tail, head)
        // head要用来存放内容，那么nextHead需要必须为空
        // 也就是说，必须要head转一圈之后，和tail之间在放满的
        // 情况下也必须空一个空间出来。
        while (next_head == tail_.load(std::memory_order_acquire)) {
            // nothing
        }

        // 当没有放满的时候
        new (&buf_[head]) T(std::forward<Args>(args)...);
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    T *front() noexcept {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (head_.load(std::memory_order_acquire) == tail) {
            return nullptr;
        }
        return &buf_[tail];
    }

    void pop() noexcept {
        auto tail = tail_.load(std::memory_order_relaxed);
        auto next_tail = tail + 1;
        if (next_tail == cap_) {
            next_tail = 0;
        }
        buf_[tail].~T();
        tail_.store(next_tail, std::memory_order_release);
    }

    size_t size() const noexcept {
        size_t diff = head_.load(std::memory_order_acquire) -
                      tail_.load(std::memory_order_acquire);
        if (diff < 0) {
            diff += cap_;
        }
        return diff;
    }

   private:
    int cap_ = 0;
    std::atomic<int> head_{0};
    std::atomic<int> tail_{0};
    T *buf_ = nullptr;
};

class msg_chan_t {
   public:
    msg_chan_t() : q_(64) {}

   public:
    bool read_once(int *val) {
        int v;
        if (q_.front()) {
            v = *q_.front();
            q_.pop();

            if (val) {
                *val = v;
            }
            return true;
        }

        return false;
    }

    int read_stuck() {
        while (!q_.front())
            ;
        auto v = *q_.front();
        q_.pop();
        return v;
    }

    void write(int v) {
        q_.push(v);
    }

    int size() {
        return q_.size();
    }

   private:
    SPSCQueue<int> q_;
};


#endif /* ! end EX_CHAN_H_ */
