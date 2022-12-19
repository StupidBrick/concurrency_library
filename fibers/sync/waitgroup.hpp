#pragma once

#include "../awaiters.hpp"
#include "../fiber.hpp"

namespace Fibers::Sync {

    class WaitGroup {
    public:
        void Add(size_t count) {
            count_.fetch_add((long long)count, std::memory_order_release);
        }

        void Done() {
            if (count_.fetch_sub(1, std::memory_order_release) == 1) {
                auto* old_head = head_.load(std::memory_order_relaxed);
                while (!head_.compare_exchange_weak(old_head, nullptr, std::memory_order_acquire,
                                                    std::memory_order_relaxed)) {
                }
                ResumeAwaiters(old_head);
            }
        }

        void AllDone() {
            count_.store(0, std::memory_order_release);
            auto* old_head = head_.load(std::memory_order_relaxed);
            while (!head_.compare_exchange_weak(old_head, nullptr, std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
            }
            ResumeAwaiters(old_head);
        }

        void Wait() {
            if (count_.load(std::memory_order_acquire) == 0) {
                return;
            }

            std::atomic_flag resumed{ false };
            Awaiters::WaitGroupAwaiter awaiter(Fiber::Self(), resumed);
            PushInStack(&awaiter);
            Fiber::Self().Suspend(&awaiter);
        }

    private:
        static void ResumeAwaiters(Awaiters::WaitGroupAwaiter* stack) {
            if (stack == nullptr) {
                return;
            }

            auto* queue = CreateQueueFromStack(stack);

            while (queue != nullptr) {
                auto* next = (Awaiters::WaitGroupAwaiter*)queue->next;
                queue->Resume();
                queue = next;
            }
        }

        static Awaiters::WaitGroupAwaiter* CreateQueueFromStack(Awaiters::WaitGroupAwaiter* stack) {
            auto* next_queue_head = (Awaiters::WaitGroupAwaiter*)stack->next;
            Awaiters::WaitGroupAwaiter* queue_head = stack;
            while (next_queue_head != nullptr) {
                Awaiters::WaitGroupAwaiter* next_queue_head_copy = next_queue_head;
                next_queue_head = (Awaiters::WaitGroupAwaiter*)next_queue_head->next;
                next_queue_head_copy->next = queue_head;
                queue_head = next_queue_head_copy;
            }
            return queue_head;
        }

        Awaiters::WaitGroupAwaiter* PushInStack(Awaiters::WaitGroupAwaiter* awaiter) {
            Awaiters::WaitGroupAwaiter* result;
            result = head_.load(std::memory_order_relaxed);
            do {
                awaiter->next = result;
            } while (!head_.compare_exchange_strong(result, awaiter, std::memory_order_release,
                                                          std::memory_order_relaxed));
            return result;
        }

    private:
        std::atomic<Awaiters::WaitGroupAwaiter*> head_{ nullptr };
        std::atomic<long long> count_{ 0 };
    };

}