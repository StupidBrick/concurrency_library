#pragma once

#include "iexecutor.hpp"
#include <atomic>
#include "../intrusive/structures/stack.hpp"
#include <cassert>

namespace Executors {

    // Async mutex
    class Strand : public IExecutor {
    private:
        class TasksBatch : public Intrusive::TaskBase {
        public:
            TasksBatch() = default;

            TasksBatch(Intrusive::Stack stack, Strand* strand) : stack_(std::move(stack)), strand_(strand) {
            }

            bool AllocatedOnHeap() override {
                return false;
            }

            void Run() override {
                Routine* routine;
                while ((routine = (Routine*)stack_.TryPop()) != nullptr) {
                    bool need_to_delete = routine->AllocatedOnHeap();
                    routine->Run();
                    if (need_to_delete) {
                        routine->Discard();
                    }
                }

                strand_->Unlock();
            }

            void Discard() override {
                assert(false);
            }

        private:
            Intrusive::Stack stack_;
            Strand* strand_ = nullptr;
        };

    public:
        friend TasksBatch;

        explicit Strand(IExecutor& executor) : executor_(&executor) {
        }

        Strand() = default;

        void SetExecutor(IExecutor& executor) {
            executor_ = &executor;
        }

        void Execute(Routine* routine) override;

        void YieldExecute(Routine* routine) override {
            Execute(routine);
        }

    private:
        Routine* PushInStack(Routine* routine);

        void Lock();

        void Unlock();

        // make queue of 2 stacks
        static Intrusive::Stack MakeQueue(Routine* head);

    private:
        // because pointer occupied only last 48 bits
        static const uint64_t kClosed = ((uint64_t)1 << 63);
        IExecutor* executor_ = nullptr;
        std::atomic<Intrusive::SinglyDirectedListNode*> stack_head_{ nullptr };

        // To avoid extra allocations
        TasksBatch batch_;
    };

    Intrusive::Stack Strand::MakeQueue(Routine *head) {
        Intrusive::Stack result;
        while (head != nullptr && head != (Intrusive::SinglyDirectedListNode*)kClosed) {
            auto* new_head = (Routine*)head->next;
            result.Push(head);
            head = new_head;
        }
        return std::move(result);
    }

    void Strand::Execute(Routine *routine) {
        Routine* result = PushInStack(routine);
        if (result == nullptr) {
            Lock();
        }
    }

    Routine *Strand::PushInStack(Routine *routine) {
        routine->next = stack_head_.load(std::memory_order_relaxed);
        Intrusive::SinglyDirectedListNode* result;
        do {
            result = routine->next;
        } while (!stack_head_.compare_exchange_strong(routine->next, routine, std::memory_order_release,
                                                      std::memory_order_relaxed));
        return (Routine*)result;
    }

    void Strand::Lock() {
        auto* head = stack_head_.load(std::memory_order_relaxed);
        while (!stack_head_.compare_exchange_weak(head, (Intrusive::SinglyDirectedListNode*)kClosed,
                                                  std::memory_order_acquire,
                                                  std::memory_order_relaxed)) {
        }

        Intrusive::Stack queue = std::move(MakeQueue((Routine*)head));
        batch_ = std::move(TasksBatch(std::move(queue), this));

        executor_->Execute(&batch_);
    }

    void Strand::Unlock() {
        auto* closed = (Intrusive::SinglyDirectedListNode*)kClosed;
        if (stack_head_.compare_exchange_strong(closed, nullptr, std::memory_order_relaxed)) {
            return; // queue is empty
        }

        Lock();
    }

}