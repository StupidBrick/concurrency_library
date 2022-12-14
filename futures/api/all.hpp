#pragma once
#include <vector>
#include "../future.hpp"

namespace Futures {

    // nonblocking wait for the result of all futures
    template <typename T>
    SemiFuture<std::vector<typename Detail::ChangeVoidOnMonostate<T>::Type>> All(std::vector<Future<T>> inputs) {
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;
        auto [f, p] = MakeContract<std::vector<FutureType>>();

        struct VectorPromise {
            Promise<std::vector<FutureType>> promise;
            std::vector<Result<FutureType>> promise_value;

            std::atomic<int> count;

            VectorPromise(Promise<std::vector<FutureType>> promise,
                          int count) : promise(std::move(promise)),
                                       count(count), promise_value(count) {
            }
        };

        auto* promise = new VectorPromise(std::move(p), inputs.size());

        for (int i = 0; i < (int)inputs.size(); ++i) {
            std::move(inputs[i]).Subscribe([promise = promise, index = i](Result<FutureType> result) mutable {
                promise->promise_value[index] = std::move(result);

                if (promise->count.fetch_sub(1, std::memory_order_acq_rel) != 1) {
                    return;
                }

                // Here we get all results
                Result<std::vector<FutureType>> promise_result;
                try {
                    std::vector<FutureType> vec;
                    for (auto& el : promise->promise_value) {
                        vec.emplace_back(std::move(el.ValueOrThrow()));
                    }
                    promise_result.SetValue(std::move(vec));
                }
                catch (...) {
                    promise_result.SetException(std::current_exception());
                }
                std::move(promise->promise).Set(std::move(promise_result));
                delete promise;
            });
        }

        return std::move(f);
    }

    template <typename T, typename... Futures>
    SemiFuture<std::vector<typename Detail::ChangeVoidOnMonostate<T>::Type>> All(Future<T>&& first, Futures&&... other) {
        return std::move(All(std::move(Detail::GetVector(std::forward<Future<T>>(first),
                                                         std::forward<Futures>(other)...))));
    }

}