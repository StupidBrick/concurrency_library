#pragma once

#include "fiber_handle.hpp"
#include "../intrusive/tasks/default_task.hpp"
#include "iawaiter.hpp"
#include "../futures/future.hpp"

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

}