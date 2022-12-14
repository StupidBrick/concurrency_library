#pragma once
#include "icallback.hpp"

namespace Futures {

    template <typename T, typename Functor>
    class SubscribeCallback : public ICallback<T> {
    public:
        using ValueType = T;
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;

        using Calculate = typename Detail::CalculateFunction</*Functor=*/Functor,
                                                             /*ReturnType=*/void,
                                                             /*ArgType=*/Result<FutureType>,
                                                             /*FutureType=*/Result<FutureType>>;

        explicit SubscribeCallback(Functor func) : func_(std::move(func)) {
        }

        void Invoke(Result<FutureType>&& result) noexcept override {
            try {
                Calculate::Calculate(std::move(func_), std::move(result));
                delete this;
            }
            catch(...) {
                delete this;
                std::rethrow_exception(std::current_exception());
            }
        }

    private:
        Functor func_;
    };

}