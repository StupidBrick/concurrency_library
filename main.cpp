#include "executors/thread_pool/with_waitidle/thread_pool.hpp"
#include "executors/api.hpp"
#include "lockfree/queue.hpp"
#include "executors/manual_executor.hpp"
#include "lockfree/stack.hpp"

int main() {
    Executors::WithWaitIdle::ThreadPool tp{ 2 };
    // Executors::ManualExecutor tp;

    LockFree::Queue<int> queue;

    std::atomic<int> count{ 0 };

    for (size_t i = 0; i < 10000; ++i) {
        Executors::Execute(tp, [&queue, i]() mutable {
            queue.Push((int)i);
        });

        Executors::Execute(tp, [&queue, &count]() {
            if (queue.TryPop() == std::nullopt) {
                count.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    tp.WaitIdle();
    tp.Stop();

    std::cout << count << '\n';
}