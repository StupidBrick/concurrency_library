#pragma once

#include "fiber_handle.hpp"
#include "../intrusive/tasks/default_task.hpp"
#include "iawaiter.hpp"
#include "../futures/future.hpp"
#include "../detail/spinlock.hpp"
#include "../intrusive/structures/singly_directed_list_node.hpp"
#include "../intrusive/structures/bidirectional_list_node.hpp"
#include "../intrusive/structures/list.hpp"
#include <optional>

namespace Fibers::Awaiters {

    class DefaultAwaiter : public IAwaiter {
        void AwaitSuspend() override {
        }
    };

    class ReScheduleAwaiter : public IAwaiter {
    public:
        explicit ReScheduleAwaiter(FiberHandle handle) : handle_(handle) {
        }

        void AwaitSuspend() override {
            handle_.Schedule();
        }

    private:
        FiberHandle handle_;
    };

    class YieldAwaiter : public IAwaiter {
    public:
        explicit YieldAwaiter(FiberHandle handle) : handle_(handle) {
        }

        void AwaitSuspend() override {
            handle_.YieldSchedule();
        }

    private:
        FiberHandle handle_;
    };

    class MutexAwaiter : public IAwaiter, public Intrusive::SinglyDirectedListNode {
    public:
        MutexAwaiter(FiberHandle handle, Detail::QueueSpinLock::Guard& guard) : handle_(handle), guard_(guard) {
        }

        void AwaitSuspend() override {
            guard_.Unlock();
        }

        void Resume() {
            handle_.Schedule();
        }

    private:
        FiberHandle handle_;
        Detail::QueueSpinLock::Guard& guard_;
    };

    class WaitGroupAwaiter : public IAwaiter, public Intrusive::SinglyDirectedListNode {
    public:
        WaitGroupAwaiter(FiberHandle handle, std::atomic_flag& resumed) : handle_(handle), resumed_(resumed) {
        }

        void AwaitSuspend() override {
            if (resumed_.test_and_set(std::memory_order_acq_rel)) {
                handle_.Schedule();
            }
        }

        void Resume() {
            if (resumed_.test_and_set(std::memory_order_acq_rel)) {
                handle_.Schedule();
            }
        }

    private:
        FiberHandle handle_;
        std::atomic_flag& resumed_;
    };

    template <typename T, typename ResultType>
    class FutureAwaiter : public IAwaiter {
    public:
        FutureAwaiter(FiberHandle handle,
                      Futures::Future<T> future,
                      Futures::Result<ResultType>& result) :
                      handle_(handle), future_(std::move(future)), result_(result) {
        }

        void AwaitSuspend() override {
            std::move(future_).Subscribe([this](Futures::Result<ResultType> result) {
                result_ = std::move(result);
                handle_.Schedule();
            });
        }

    private:
        FiberHandle handle_;
        Futures::Future<T> future_;
        Futures::Result<ResultType>& result_;
    };

    template <typename T>
    class ChannelProducerAwaiter : public IAwaiter, public Intrusive::SinglyDirectedListNode {
    public:
        ChannelProducerAwaiter(FiberHandle handle, T&& result,
                               ::Detail::QueueSpinLock::Guard& guard) :
                               handle_(handle), result_(std::forward<T>(result)), guard_(guard) {
        }

        void AwaitSuspend() override {
            guard_.Unlock();
        }

        T Resume() {
            handle_.Schedule();
            return std::move(result_);
        }

    private:
        ::Detail::QueueSpinLock::Guard& guard_;
        FiberHandle handle_;
        T result_;
    };

    class ChannelConsumerAwaiterBase : public IAwaiter, public Intrusive::BidirectionalListNode {
    public:
        virtual void SetQueue(Intrusive::List* queue) {
        }

        virtual void SetIndex(int index) {
        }
    };

    template <typename T>
    class IChannelConsumerAwaiter : public ChannelConsumerAwaiterBase {
    public:
        virtual void Resume(T&&) = 0;
    };

    template <typename T>
    class ChannelConsumerAwaiter : public IChannelConsumerAwaiter<T> {
    public:
        ChannelConsumerAwaiter(FiberHandle fiber, std::optional<T>& result,
                               ::Detail::QueueSpinLock::Guard& guard) : fiber_(fiber), result_(result), guard_(guard) {
        }

        void AwaitSuspend() override {
            guard_.Unlock();
        }

        void Resume(T&& result) override {
            result_ = std::forward<T>(result);
            fiber_.Schedule();
        }

    private:
        FiberHandle fiber_;
        std::optional<T>& result_;
        ::Detail::QueueSpinLock::Guard& guard_;
    };

    template <typename T, typename Variant, size_t Size>
    class SelectorAwaiter : public IChannelConsumerAwaiter<T> {
    private:
        using AwaitersArray = std::array<std::pair<Intrusive::BidirectionalListNode*, Intrusive::List*>, Size>;

    public:
        SelectorAwaiter(FiberHandle fiber, Variant& result, AwaitersArray& awaiters, std::atomic_flag& is_result_set) :
        fiber_(fiber), result_(result), awaiters_(awaiters), is_result_set_(is_result_set) {
            awaiters_[index_] = { this, nullptr };
        }

        void AwaitSuspend() override {
            if (is_result_set_.test_and_set(std::memory_order_acquire)) {
                fiber_.Schedule();
            }
        }

        void Resume(T&& result) override {
            result_ = std::move(result);
            for (size_t i = 0; i < Size; ++i) {
                if (i != index_ && awaiters_[i].second != nullptr) {
                    awaiters_[i].second->Unlink(awaiters_[i].first);
                }
            }
            if (is_result_set_.test_and_set(std::memory_order_release)) {
                fiber_.Schedule();
            }
        }

        void SetQueue(Intrusive::List* list) override {
            awaiters_[index_] = { awaiters_[index_].first, list };
        }

        void SetIndex(int index) override {
            index_ = index;
        }

    private:
        FiberHandle fiber_;
        Variant& result_;
        AwaitersArray& awaiters_;
        size_t index_ = 0;
        std::atomic_flag& is_result_set_;
    };

}