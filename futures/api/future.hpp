#pragma once

#include "../future.hpp"

namespace Futures {

    // usage : auto [f, p] = Futures::MakeContract<T>();
    template <typename T>
    Contract<T> MakeContract() {
        return Contract<T>(new FutureValue<T>);
    }

    // blocking waiting for the result
    template <typename T>
    Result<typename Detail::ChangeVoidOnMonostate<T>::Type> GetResult(Future<T>&& future) {
        future.value_->callback.wait(nullptr);
        return std::move(future.value_->value);
    }

    template <typename T>
    Result<typename Detail::ChangeVoidOnMonostate<T>::Type> GetResult(SemiFuture<T>&& future) {
        future.value_->callback.wait(nullptr);
        return std::move(future.value_->value);
    }

}