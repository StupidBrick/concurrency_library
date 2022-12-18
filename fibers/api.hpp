#pragma once

#include "../executors/iexecutor.hpp"
#include <functional>
#include "awaiters.hpp"
#include "../futures/api/future.hpp"
#include "fiber.hpp"
#include "../futures/detail/type_traits.hpp"

namespace Fibers {

    void Go(Executors::IExecutor& sched, std::function<void()> routine) {
        auto* fiber = new Fiber(std::move(routine), sched);
        fiber->Schedule();
    }

    template <typename Functor>
    auto AsyncVia(Executors::IExecutor& sched, Functor functor) {
        using ReturnType = std::invoke_result_t<Functor>;
        auto [f, p] = Futures::MakeContract<ReturnType>();

        std::shared_ptr<Futures::Promise<ReturnType>> promise_ptr =
                std::make_shared<Futures::Promise<ReturnType>>(std::move(p));

        Go(sched, [promise_ptr, functor = std::move(functor)]() mutable {
            Futures::Result<typename Futures::Detail::ChangeVoidOnMonostate<ReturnType>::Type> result;
            try {
                using Calculate = typename Futures::Detail::CalculateFunction</*Functor=*/Functor,
                        /*ReturnType=*/ReturnType,
                        /*ArgType=*/void,
                        /*FutureType=*/std::monostate>;
                result.SetValue(std::move(Calculate::Calculate(std::move(functor), std::monostate())));
            }
            catch (...) {
                result.SetException(std::current_exception());
            }

            std::move(*promise_ptr).Set(std::move(result));
        });

        return std::move(f);
    }

    namespace Self {

        void Suspend(Awaiters::IAwaiter* awaiter) {
            Fiber::Self().Suspend(awaiter);
        }

        void Yield() {
            Awaiters::YieldAwaiter awaiter(Fiber::Self());
            Suspend(&awaiter);
        }

        // non-blocking future wait
        template <typename T>
        auto Await(Futures::Future<T>&& future) {
            using ResultType = typename Futures::Detail::ChangeVoidOnMonostate<T>::Type;

            if (future.IsReady()) {
                return Futures::GetResult(std::move(future));
            }

            typename Futures::Result<ResultType> result;

            auto current = Fiber::Self();

            Awaiters::FutureAwaiter<T, ResultType> awaiter(current, std::move(future), result);
            current.Suspend(&awaiter);

            return std::move(result);
        }

        template <typename T>
        auto Await(Futures::SemiFuture<T>&& future) {
            return Await(std::move(future).Via(Fiber::Self().GetScheduler()));
        }

    }

}