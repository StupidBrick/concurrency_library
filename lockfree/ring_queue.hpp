#pragma once

#include <array>
#include <span>

#include <atomic>

namespace LockFree {

    // single-Producer / multi-Consumer Bounded Ring Buffer
    template <typename T, size_t Capacity>
    class RingQueue {
        struct Slot {
            std::atomic<T*> item;
        };

    public:
        bool TryPush(T* item) {
            uint64_t head = head_.load(std::memory_order_relaxed);
            uint64_t old_tail = tail_.load(std::memory_order_relaxed);

            if (old_tail - head == Capacity) {
                return false;
            }

            buffer_[old_tail % (Capacity + 1)].item.store(item,
                                                          std::memory_order_relaxed);
            tail_.fetch_add(1, std::memory_order_release);

            return true;
        }

        // returns nullptr if queue is empty
        T* TryPop() {
            uint64_t old_head, tail;
            T* result;
            old_head = head_.load(std::memory_order_relaxed);
            do {
                tail = tail_.load(std::memory_order_acquire);

                if (old_head == tail) {
                    return nullptr;
                }

                result = buffer_[old_head % (Capacity + 1)].item.
                        load(std::memory_order_relaxed);
            } while(!head_.compare_exchange_strong(old_head, old_head + 1,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed));
            return result;
        }

        template <typename Queue>
        size_t Grab(Queue& queue, size_t size) {
            uint64_t head = head_.load(std::memory_order_relaxed);
            size_t grabbed_cnt;

            // we start copy tasks naively
            // and if sequence of slots was affected by other threads
            // then we start again
            // and loop until sequence weren't changed.

            do {
                queue.Clear();
                uint64_t head_pos = head % (Capacity + 1);
                auto next_tail = tail_.load(std::memory_order_acquire);
                grabbed_cnt = std::min(next_tail - head, size);

                for (size_t i = 0; i < grabbed_cnt; ++i) {

                    // there is no situation for one atomic with two ambiguous writes
                    queue.Push(buffer_[head_pos].item.load(std::memory_order_relaxed));
                    head_pos = ((head_pos + 1) == (Capacity + 1) ? 0 : head_pos + 1);
                }
            } while (!head_.compare_exchange_strong(head, head + grabbed_cnt,
                                                    std::memory_order_relaxed,
                                                    std::memory_order_relaxed));

            return grabbed_cnt;
        }

    private:
        std::array<Slot, Capacity + 1> buffer_;

        // count of successful push and pop
        std::atomic<uint64_t> head_{ 0 };
        std::atomic<uint64_t> tail_{ 0 };
    };

}
