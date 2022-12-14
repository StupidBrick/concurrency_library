#pragma once

#include "../intrusive/tasks/task_base.hpp"

namespace Executors {

    using Routine = Intrusive::TaskBase;

    class IExecutor {
    public:
        virtual ~IExecutor() = default;

        virtual void Execute(Routine* routine) = 0;

        // scheduling after yield may differ
        virtual void YieldExecute(Routine* routine) = 0;
    };

}