#pragma once

#include "future.hpp"

namespace Futures {

    // usage : auto f = Futures::Execute(exec, []() -> T { return T; })
    template <typename Functor>
    auto Execute(Executors::IExecutor& executor, Functor functor) {
        using ReturnType = std::invoke_result_t<Functor>;
        auto [f, p] = MakeContract<ReturnType>();


        Executors::Execute(executor, [promise = std::move(p), functor = std::move(functor)]() mutable {
            // callback will delete itself
            Result<typename Detail::ChangeVoidOnMonostate<ReturnType>::Type> result;
            try {
                using Calculate = typename Detail::CalculateFunction</*Functor=*/Functor,
                        /*ReturnType=*/ReturnType,
                        /*ArgType=*/void,
                        /*FutureType=*/std::monostate>;
                result.SetValue(std::move(Calculate::Calculate(std::move(functor), std::monostate())));
            }
            catch (...) {
                result.SetException(std::current_exception());
            }

            std::move(promise).Set(std::move(result));
        });

        return std::move(f);
    }
    // if std::invoke_result_t<Functor> != void ReturnType = Future<std::invoke_result_t<Functor>>,
    // else ReturnType = Future<std::monostate>

}