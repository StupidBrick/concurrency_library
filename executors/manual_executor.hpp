#pragma once

#include "iexecutor.hpp"
#include "../intrusive/structures/queue.hpp"
#include <iostream>

namespace Executors {
    // single-threaded executor
    class ManualExecutor : public IExecutor {
    public:
        void Execute(Routine* routine) override {
            tasks_queue_.Push(routine);
            ++queue_size_;
        }

        void YieldExecute(Routine* routine) override {
            Execute(routine);
        }

        size_t RunAtMost(size_t limit) {
            size_t result = std::min(queue_size_, limit);
            for (size_t i = 0; i < result; ++i) {
                auto* routine = (Routine*)tasks_queue_.TryPop();
                bool need_to_delete = routine->AllocatedOnHeap();
                routine->Run();
                if (need_to_delete) {
                    routine->Discard();
                }
            }
            queue_size_ -= result;
            return result;
        }

        bool RunNext() {
            return (RunAtMost(1) == 1);
        }

        size_t Drain() {
            return RunAtMost(queue_size_);
        }

        size_t WaitIdle() {
            size_t result = 0;
            while (RunNext()) {
                ++result;
            }
            return result;
        }

        [[nodiscard]] size_t TaskCount() const {
            return queue_size_;
        }

        [[nodiscard]] bool HasTasks() const {
            return (queue_size_ == 0);
        }

        ~ManualExecutor() noexcept override {
            Routine* routine;
            while ((routine = (Routine*)tasks_queue_.TryPop()) != nullptr) {
                if (routine->AllocatedOnHeap()) {
                    routine->Discard();
                }
            }
        }

    private:
        Intrusive::Queue tasks_queue_;
        size_t queue_size_ = 0;
    };

}
