#pragma once
#include "future_value.hpp"
#include "../executors/api.hpp"

namespace Futures {

    template<typename T>
    class Promise {
    public:
        template<typename U>
        friend struct Contract;

        // if T != void : FutureType = T; else FutureType == std::monostate
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;
        using ValueType = T;

        Promise(const Promise &) = delete;

        Promise &operator=(const Promise &) = delete;

        Promise(Promise &&other) noexcept;

        Promise &operator=(Promise &&other) noexcept;

        void Set(Result <FutureType> &&result) &&;

        void SetValue(FutureType value) &&;

        void SetValue() &&;

        void SetException(std::exception_ptr exception) &&;

        ~Promise() noexcept;

    private:
        explicit Promise(FutureValue <ValueType> *value);

    private:
        FutureValue <ValueType> *value_;
    };


    template <typename T>
    Promise<T>::Promise(Promise&& other) noexcept {
        value_ = other.value_;
        other.value_ = nullptr;
    }

    template <typename T>
    Promise<T>& Promise<T>::operator=(Promise&& other) noexcept {
        if (value_ != nullptr) {
            value_->Destroy();
        }
        value_ = other.value_;
        other.value_ = nullptr;
        return *this;
    }

    template <typename T>
    void Promise<T>::Set(Result<typename Promise<T>::FutureType>&& result) && {
        // TODO set memory orders
        value_->value = std::move(result);

        auto* old_callback = value_->callback.load(std::memory_order_relaxed);
        while (!value_->callback.compare_exchange_weak(old_callback, (ICallback<T>*)FutureValue<T>::kValueIsSet)) {
        }

        if (old_callback != nullptr) {
            // value is set
            Executors::Execute(*value_->executor, [value = std::move(value_->value), callback = old_callback]() mutable {
                // callback will delete itself
                callback->Invoke(std::move(value));
            });
        }
        else {
            // for GetResult function
            value_->callback.notify_one();
        }
    }

    template <typename T>
    void Promise<T>::SetValue(FutureType value) && {
        static_assert(!std::is_same_v<void, ValueType>);
        Result<FutureType> result;
        result.SetValue(std::move(value));
        std::move(*this).Set(std::move(result));
    }

    template <typename T>
    void Promise<T>::SetValue() && {
        static_assert(std::is_same_v<void, ValueType>);
        Result<std::monostate> result;
        result.SetValue(std::monostate());
        std::move(*this).Set(std::move(result));
    }

    template <typename T>
    void Promise<T>::SetException(std::exception_ptr exception) &&{
        Result<FutureType> result;
        result.SetException(exception);
        std::move(*this).Set(std::move(result));
    }

    template <typename T>
    Promise<T>::~Promise() noexcept {
        if (value_ != nullptr) {
            value_->Destroy();
        }
    }

    template <typename T>
    Promise<T>::Promise(FutureValue<typename Promise<T>::ValueType>* value) {
        value_ = value;
    }


}