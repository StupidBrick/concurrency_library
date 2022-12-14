#pragma once

#include "../executors/iexecutor.hpp"

namespace Fibers {

    class Fiber;

    namespace Awaiters {
        class IAwaiter;
    }

    namespace Sync {
        class Mutex;
    }

    class FiberHandle {
    public:
        friend Fiber;
        friend Sync::Mutex;

        void Schedule();

        void YieldSchedule();

        void Suspend(Awaiters::IAwaiter* awaiter);

        void Resume();

        Executors::IExecutor& GetScheduler();

        bool IsValid();

    private:
        explicit FiberHandle(Fiber* fiber) : fiber_(fiber) {
        }

        Fiber* fiber_;
    };

}