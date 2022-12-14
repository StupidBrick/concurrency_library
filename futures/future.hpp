#pragma once

#include "../executors/iexecutor.hpp"
#include "future_value.hpp"
#include <variant>
#include "detail/type_traits.hpp"
#include "callbacks/then_callback.hpp"
#include "callbacks/subscribe_callback.hpp"
#include "callbacks/recover_callback.hpp"

namespace Futures {

    template <typename ReturnType>
    class SemiFuture;

    template <typename T>
    struct Contract {
        SemiFuture<T> future;
        Promise<T> promise;

        explicit Contract(FutureValue<T>* value) : future(value), promise(value) {
        }
    };

    template <typename T>
    class Future {
    public:
        template <typename U>
        friend Result<typename Detail::ChangeVoidOnMonostate<U>::Type> GetResult(Future<U>&& future);

        friend SemiFuture<T>;

        // if T != void : FutureType = T; else FutureType == std::monostate
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;
        using ValueType = T;

        Future(const Future&) = delete;
        Future& operator=(const Future&) = delete;

        Future(Future&& other) noexcept;

        Future& operator=(Future&& other) noexcept;

        template <typename Functor>
        auto Then(Functor functor) && {
            using ReturnType = typename Detail::GetReturnType<Functor, ValueType>::Type;
            return std::move(ThenImpl<ReturnType, Functor>(std::move(functor)));
        }

        template <typename Functor>
        SemiFuture<ValueType> Recover(Functor functor) &&;

        template <typename Functor>
        void Subscribe(Functor callback) &&;

        [[nodiscard]] bool IsReady() const;

        ~Future() noexcept;

    private:
        Future(Executors::IExecutor& executor, FutureValue<ValueType>* value);

        template <typename ReturnType, typename Functor>
        SemiFuture<ReturnType> ThenImpl(Functor functor);

        void SetCallback(ICallback<T>* callback);

    private:
        FutureValue<ValueType>* value_;
    };

    template <typename T>
    class SemiFuture {
    public:
        template <typename U>
        friend class Future;

        template <typename U>
        friend struct Contract;

        template <typename U>
        friend Result<typename Detail::ChangeVoidOnMonostate<U>::Type> GetResult(SemiFuture<U>&& future);

        // if T != void : FutureType = T; else FutureType == std::monostate
        using FutureType = typename Detail::ChangeVoidOnMonostate<T>::Type;
        using ValueType = T;

        SemiFuture(const SemiFuture&) = delete;
        SemiFuture& operator=(const SemiFuture&) = delete;

        SemiFuture(SemiFuture&& other) noexcept;

        SemiFuture& operator=(SemiFuture&& other) noexcept;

        Future<T> Via(Executors::IExecutor& executor) &&;

        [[nodiscard]] bool IsReady() const;

        ~SemiFuture() noexcept;

    private:
        explicit SemiFuture(FutureValue<ValueType>* value);

    private:
        FutureValue<ValueType>* value_;
    };

    //// FUTURE
    template <typename T>
    Future<T>::Future(Future&& other) noexcept {
        value_ = other.value_;
        other.value_ = nullptr;
    }

    template <typename T>
    Future<T>& Future<T>::operator=(Future&& other) noexcept {
        if (value_ != nullptr) {
            value_->Destroy();
        }
        value_ = other.value_;
        other.value_ = nullptr;
        return *this;
    }

    template <typename T>
    template <typename Functor>
    SemiFuture<typename Future<T>::ValueType> Future<T>::Recover(Functor functor) && {
        auto [f, p] = std::move(Contract<ValueType>(new FutureValue<ValueType>)); //MakeContract<ValueType>();

        auto* callback = new RecoverCallback<T, Functor>(std::move(functor), std::move(p));
        SetCallback(callback);

        return std::move(f);
    }

    template <typename T>
    template <typename Functor>
    void Future<T>::Subscribe(Functor callback) && {
        ICallback<ValueType>* callback_ptr = new SubscribeCallback<ValueType, Functor>(std::move(callback));
        SetCallback(callback_ptr);
    }

    template <typename T>
    void Future<T>::SetCallback(ICallback<T> *callback) {
        // TODO set memory orders
        auto* old_callback = value_->callback.load(std::memory_order_relaxed);
        while (!value_->callback.compare_exchange_weak(old_callback, callback)) {
        }

        if ((uint64_t)old_callback == FutureValue<T>::kValueIsSet) {
            // value is set
            Executors::Execute(*value_->executor, [value = std::move(value_->value), callback = callback]() mutable {
                // callback will delete itself
                callback->Invoke(std::move(value));
            });
        }
    }

    template <typename T>
    Future<T>::~Future() noexcept {
        if (value_ != nullptr) {
            value_->Destroy();
        }
    }

    template <typename T>
    Future<T>::Future(Executors::IExecutor& executor, FutureValue<ValueType>* value) {
        value_ = value;
        value_->executor = &executor;
    }

    template <typename T>
    bool Future<T>::IsReady() const {
        return value_->callback.load(std::memory_order_relaxed) != nullptr;
    }

    template <typename T>
    template <typename ReturnType, typename Functor>
    SemiFuture<ReturnType> Future<T>::ThenImpl(Functor functor) {
        auto [f, p] = std::move(Contract<ReturnType>(new FutureValue<ReturnType>)); // MakeContract<ReturnType>();

        auto* callback = new ThenCallback<T, Functor>(std::move(functor), std::move(p));
        SetCallback(callback);

        return std::move(f);
    }


    //// SEMIFUTURE
    template <typename T>
    SemiFuture<T>::SemiFuture(SemiFuture&& other) noexcept {
        value_ = other.value_;
        other.value_ = nullptr;
    }

    template <typename T>
    SemiFuture<T>& SemiFuture<T>::operator=(SemiFuture&& other) noexcept {
        if (value_ != nullptr) {
            value_->Destroy();
        }
        value_ = other.value_;
        other.value_ = nullptr;
        return *this;
    }

    template <typename T>
    Future<T> SemiFuture<T>::Via(Executors::IExecutor& executor) && {
        auto* value_copy = value_;
        value_ = nullptr;
        return { executor, value_copy };
    }

    template <typename T>
    SemiFuture<T>::~SemiFuture() noexcept {
        if (value_ != nullptr) {
            value_->Destroy();
        }
    }

    template <typename T>
    bool SemiFuture<T>::IsReady() const {
        return value_->callback.load(std::memory_order_relaxed) != nullptr;
    }

    template <typename T>
    SemiFuture<T>::SemiFuture(FutureValue<typename SemiFuture<T>::ValueType>* value) {
        value_ = value;
    }

}
