#pragma once

#include <exception>
#include <memory>

namespace Futures {

    template <typename T>
    class ValueStorage {
    public:
        using ValueType = T;

        ValueStorage() = default;

        template <typename ...Args>
        explicit ValueStorage(Args&&... args) {
            value_ = std::make_unique<T>(std::forward<Args>(args)...);
        }

        explicit ValueStorage(T&& value) {
            value_ = std::make_unique<T>(std::forward<T>(value));
        }

        ValueStorage(const ValueStorage&) = delete;
        ValueStorage& operator=(const ValueStorage&) = delete;

        ValueStorage(ValueStorage&&) noexcept = default;
        ValueStorage& operator=(ValueStorage&&) noexcept = default;

        [[nodiscard]] bool HasValue() const {
            return (value_ != nullptr);
        }

        void SetValue(T&& value) {
            if (HasValue()) {
                *value_ = std::forward<T>(value);
            }
            else {
                value_ = std::make_unique<T>(std::forward<T>(value));
            }
        }

        T& GetValue() {
            return *value_;
        }

        const T& GetValue() const {
            return *value_;
        }

        T GetValueAndDestroy() {
            T result = std::move(*value_);
            value_ = nullptr;
            return std::move(result);
        }

        void Clear() {
            value_ = nullptr;
        }

        ~ValueStorage() noexcept = default;

    private:
        std::unique_ptr<T> value_{ nullptr };
    };


    template <typename T>
    class Result {
    public:
        using ValueType = T;

        [[nodiscard]] bool HasValue() {
            return value_.HasValue();
        }

        [[nodiscard]] bool HasException() {
            return (exception_ != nullptr);
        }

        void SetException(std::exception_ptr exception) {
            exception_ = std::move(exception);
        }

        void SetValue(T&& value) {
            value_.SetValue(std::forward<T>(value));
        }

        template <typename ...Args>
        void EmplaceValue(Args&&... args) {
            value_.EmplaceValue(std::forward<Args>(args)...);
        }

        [[nodiscard]] std::exception_ptr GetException() const {
            return exception_;
        }

        T ValueOrThrow() {
            if (exception_ != nullptr) {
                std::rethrow_exception(exception_);
            }
            return std::move(value_.GetValueAndDestroy());
        }

        [[nodiscard]] bool Ready() {
            return (HasValue() xor HasException());
        }

    private:
        ValueStorage<T> value_;
        std::exception_ptr exception_;
    };

}