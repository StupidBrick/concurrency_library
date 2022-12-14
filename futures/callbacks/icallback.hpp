#pragma once
#include "../result.hpp"
#include "../detail/type_traits.hpp"

namespace Futures {

    template<typename T>
    class ICallback {
    public:
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;

        virtual ~ICallback() = default;

        virtual void Invoke(Result <FutureType> &&) noexcept = 0;
    };

}