#pragma once

#include <atomic>

namespace Detail {

    class WaitGroup {
    public:
        void Add(size_t count) {
            count_.fetch_add(count, std::memory_order_release);
        }

        void Done() {
            if (count_.fetch_sub(1, std::memory_order_relaxed) == 1) {
                count_.notify_all();
            }
        }

        void Wait() {
            size_t old_count = count_.load(std::memory_order_acquire);
            while (old_count != 0) {
                count_.wait(old_count, std::memory_order_relaxed);
                old_count = count_.load(std::memory_order_acquire);
            }
        }

        void AllDone() {
            count_.store(0, std::memory_order_release);
            count_.notify_all();
        }

    private:
        std::atomic<size_t> count_{ 0 };
    };

}
