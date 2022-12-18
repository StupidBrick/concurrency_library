#pragma once

#include "../coroutines/stackful/coroutine.hpp"
#include "../executors/iexecutor.hpp"
#include "../intrusive/tasks/default_task.hpp"
#include "iawaiter.hpp"
#include "awaiters.hpp"
#include <functional>
#include "fiber_handle.hpp"

namespace Fibers {
    using Coroutines::Stackful::Coroutine;
    using Executors::Routine;

    class Fiber;

    template <typename Functor>
    class FiberTask : public Intrusive::TaskBase {
    public:
        friend Fiber;

        FiberTask(Functor func, Fiber* fiber) : func_(std::move(func)), owning_fiber_(fiber) {
        }

        bool AllocatedOnHeap() override {
            return true;
        }

        void Run() override {
            func_();
        }

        void Discard() override;

    private:
        Functor func_;
        Fiber* owning_fiber_;
        bool need_to_delete_after_discard_ = true;
    };


    class FiberFunctor {
    public:
        FiberFunctor(Coroutine* coroutine, Awaiters::IAwaiter** awaiter,
                Fiber* fiber) : coroutine_(coroutine), awaiter_(awaiter), fiber_(fiber) {
        }

        void operator()();

    private:
        Coroutine* coroutine_;
        Awaiters::IAwaiter** awaiter_;
        Fiber* fiber_;
    };


    class Fiber {
    public:
        explicit Fiber(std::function<void()> routine, Executors::IExecutor& executor);

        void Schedule();

        void YieldSchedule();

        void Suspend(Awaiters::IAwaiter* awaiter);

        Executors::IExecutor& GetScheduler();

        static FiberHandle Self();

    private:
        Coroutine coroutine_;
        Awaiters::IAwaiter* awaiter_ = nullptr;

        // to avoid extra memory allocations
        FiberTask<FiberFunctor> step_{ FiberFunctor(&coroutine_, &awaiter_, this), this };
        Executors::IExecutor* executor_ = nullptr;
    };

    template <typename Functor>
    void FiberTask<Functor>::Discard() {
        if (need_to_delete_after_discard_) {
            delete owning_fiber_;
        }
        else {
            need_to_delete_after_discard_ = true;
        }
    }
}