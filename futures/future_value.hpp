#pragma once

#include "result.hpp"
#include <atomic>
#include "callbacks/icallback.hpp"
#include "../executors/iexecutor.hpp"
#include "detail/type_traits.hpp"

namespace Futures {

    template <typename T>
    struct FutureValue {
        using ValueType = T;
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;

        // because pointer occupies only 48 bits
        const static uint64_t kValueIsSet = ((uint64_t)1 << 63);

        Result<FutureType> value;

        // callback have 3 state :
        // 1) nullptr : value not set, callback not set
        // 2) kValueIsSet - value is set, callback not set
        // 3) other - callback is set, value not set
        std::atomic<ICallback<ValueType>*> callback = nullptr;

        Executors::IExecutor* executor = nullptr;

    private:
        // because only one Future and one Promise can own the FutureValue
        std::atomic<int> owners_count{ 2 };

    public:
        void Destroy() {
            if (owners_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete this;
            }
        }
    };

}