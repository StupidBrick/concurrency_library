#pragma once

#include "icallback.hpp"
#include "../promise.hpp"

namespace Futures {

    template <typename T, typename Functor>
    class RecoverCallback : public ICallback<T> {
    public:
        using ValueType = T;
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;

        using Calculate = typename Detail::CalculateFunction</*Functor=*/Functor,
                                                             /*ReturnType=*/ValueType,
                                                             /*ArgType=*/std::exception_ptr,
                                                             /*FutureType=*/std::exception_ptr>;

        explicit RecoverCallback(Functor func,
                                 Promise<ValueType> promise) : func_(std::move(func)), promise_(std::move(promise)) {
        }

        void Invoke(Result<FutureType>&& result) noexcept override {
            if (!result.HasException()) {
                std::move(promise_).Set(std::move(result));
                delete this;
                return;
            }

            Result<FutureType> res;
            try {
                res.SetValue(std::move(Calculate::Calculate(std::move(func_), res.GetException())));
            }
            catch(...) {
                res.SetException(std::current_exception());
            }
            std::move(promise_).Set(std::move(res));
            delete this;
        }

    private:
        Functor func_;
        Promise<ValueType> promise_;
    };

}