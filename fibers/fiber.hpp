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

    class Fiber {
    public:
        explicit Fiber(std::function<void()> routine, Executors::IExecutor& executor);

        void Schedule();

        void YieldSchedule();

        void Suspend(Awaiters::IAwaiter* awaiter);

        void Resume();

        Executors::IExecutor& GetScheduler();

        static FiberHandle Self();

    private:
        Coroutine coroutine_;
        Awaiters::IAwaiter* awaiter_ = nullptr;

        struct Functor_ {
            Functor_(Coroutine* coroutine, Awaiters::IAwaiter** awaiter,
                     Fiber* fiber) : coroutine_(coroutine), awaiter_(awaiter), fiber_(fiber) {
            }

            void operator()();

        private:
            Coroutine* coroutine_;
            Awaiters::IAwaiter** awaiter_;
            Fiber* fiber_;
        };

        // to avoid extra memory allocations
        Intrusive::DefaultTask<Functor_> step_{ Functor_(&coroutine_, &awaiter_, this), /*is_allocated_on_heap=*/false };
        Executors::IExecutor* executor_ = nullptr;
    };
}