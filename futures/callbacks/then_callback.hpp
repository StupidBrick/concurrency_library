#pragma once

#include "icallback.hpp"
#include "../promise.hpp"

namespace Futures {

    template <typename T, typename Functor>
    class ThenCallback : public ICallback<T> {
    public:
        using ValueType = T;
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;
        using ReturnType = typename Detail::GetReturnType<Functor, ValueType>::Type;
        using FutureReturnType = typename Detail::ChangeVoidOnMonostate<ReturnType>::Type;

        using Calculate = typename Detail::CalculateFunction</*Functor=*/Functor,
                                                             /*ReturnType=*/ReturnType,
                                                             /*ArgType=*/ValueType,
                                                             /*FutureType=*/FutureType>;

        explicit ThenCallback(Functor func,
                              Promise<ReturnType> promise) : func_(std::move(func)), promise_(std::move(promise)) {
        }

        void Invoke(Result<FutureType>&& result) noexcept override {
            Result<FutureReturnType> res;
            try {
                res.SetValue(std::move(Calculate::Calculate(std::move(func_), std::move(result.ValueOrThrow()))));
            }
            catch(...) {
                res.SetException(std::current_exception());
            }
            std::move(promise_).Set(std::move(res));
            delete this;
        }

    private:
        Functor func_;
        Promise<ReturnType> promise_;
    };



}