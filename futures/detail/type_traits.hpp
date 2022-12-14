#pragma once
#include <variant>
#include <vector>

namespace Futures::Detail {

    template <typename T>
    struct ChangeVoidOnMonostate {
        using Type = T;
    };

    template <>
    struct ChangeVoidOnMonostate<void> {
        using Type = std::monostate;
    };

    template <typename Functor, typename ReturnType, typename ArgType, typename FutureType>
    struct CalculateFunction {
        static ReturnType Calculate(Functor functor, FutureType arg) {
            return std::move(functor(std::move(arg)));
        }
    };

    template <typename Functor, typename ReturnType, typename FutureType>
    struct CalculateFunction<Functor, ReturnType, /*ArgType=*/void, FutureType> {
        static ReturnType Calculate(Functor functor, FutureType) {
            return std::move(functor());
        }
    };

    template <typename Functor, typename ArgType, typename FutureType>
    struct CalculateFunction<Functor, /*ReturnType=*/void, ArgType, FutureType> {
        static std::monostate Calculate(Functor functor, FutureType arg) {
            functor(std::move(arg));
            return {};
        }
    };

    template <typename Functor, typename FutureType>
    struct CalculateFunction<Functor, /*ReturnType=*/void, /*ArgType=*/void, FutureType> {
        static std::monostate Calculate(Functor functor, FutureType) {
            functor();
            return {};
        }
    };

    template <typename Functor, typename Arg>
    struct GetReturnType {
        using Type = typename std::invoke_result_t<Functor, Arg>;
    };

    template <typename Functor>
    struct GetReturnType<Functor, void> {
        using Type = typename std::invoke_result_t<Functor>;
    };


    template <typename T>
    void GetVector(std::vector<T>& vector, T&& first_el) {
        vector.emplace_back(std::forward<T>(first_el));
    }

    template <typename T, typename ...Args>
    void GetVector(std::vector<T>& vector, T&& first_el, Args&&... other) {
        vector.emplace_back(std::forward<T>(first_el));
        GetVector(vector, std::forward<Args>(other)...);
    }

    template <typename T, typename ...Args>
    std::vector<T> GetVector(T&& first_el, Args&&... other) {
        std::vector<T> vector;
        GetVector(vector, std::forward<T>(first_el), std::forward<Args>(other)...);
        return std::move(vector);
    }

    template <typename T>
    std::vector<T> GetVector(T&& first_el) {
        std::vector<T> result;
        result.emplace_back(std::forward<T>(first_el));
        return std::move(result);
    }


}