#include <vector>
#include "../future.hpp"

namespace Futures {

    // nonblocking wait for the first result of several futures
    template <typename T>
    SemiFuture<T> FirstOf(std::vector<Future<T>> inputs) {
        auto [f, p] = MakeContract<T>();
        using FutureType = typename Futures::Detail::ChangeVoidOnMonostate<T>::Type;

        struct FirstOfPromise {
            const int futures_count;
            Promise<T>* promise = nullptr;
            std::atomic<int> count;

            FirstOfPromise(Promise<T>&& promise, int count) : futures_count(count), count(count) {
                this->promise = new Promise<T>(std::move(promise));
            }
        };

        auto* promise = new FirstOfPromise(std::move(p), (int)inputs.size());

        for (auto& future : inputs) {
            std::move(future).Subscribe([promise = promise](Result<FutureType> result) mutable {
                auto* local_promise = promise->promise;
                int count = promise->count.fetch_sub(1, std::memory_order_acq_rel);
                if (count == promise->futures_count) {
                    // We are first
                    std::move(*local_promise).Set(std::move(result));
                    delete local_promise;
                    return;
                }

                if (count == 1) {
                    // We are last
                    delete promise;
                }
            });
        }

        return std::move(f);
    }

    template <typename T, typename... Futures>
    SemiFuture<T> FirstOf(Future<T>&& first, Futures&&... other) {
        return std::move(FirstOf(std::move(Detail::GetVector(std::forward<Future<T>>(first),
                                                             std::forward<Futures>(other)...))));
    }


}