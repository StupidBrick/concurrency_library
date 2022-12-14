#pragma once

#include "iexecutor.hpp"
#include "../intrusive/tasks/default_task.hpp"

namespace Executors {

    template <typename Functor>
    void Execute(IExecutor& executor, Functor&& task) {
        auto* intrusive_task = new Intrusive::DefaultTask<Functor>(std::forward<Functor>(task),
                /*is_allocated_on_heap=*/true);
        executor.Execute(intrusive_task);
    }

}
