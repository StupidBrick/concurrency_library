#pragma once
#include <coroutine>

namespace Tasks {

    template <typename T>
    class [[nodiscard]] Task {
    public:
        struct Promise;

        using AnyCoroHandle = std::coroutine_handle<>;
        using MyCoroHandle = std::coroutine_handle<Promise>;

        struct Promise {
            auto get_return_object() {
                // TODO
            }

            auto initial_suspend() noexcept {
                // TODO
            }

            auto final_suspend() noexcept {
                // TODO
            }

            void unhandled_exception() {
                // TODO
            }

            void return_value(T /*value*/) {
                // TODO
            }
        };

        struct Awaiter {
            bool await_ready() {
                // TODO
            }

            void await_suspend(AnyCoroHandle /*caller*/) {
                // TODO
            }

            T await_resume() {
                // TODO
            }
        };

        Task() {
            // TODO
        }

        Task(Task&&) {
            // TODO
        }

        Task& operator=(Task&&) noexcept {
            // TODO
        }

        // Non-copyable
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        ~Task() noexcept {
            // TODO
        }

        auto operator co_await() {
            // TODO
        }

    private:
        MyCoroHandle coro_;
    };

}  // namespace task

template <typename T, typename... Args>
struct std::coroutine_traits<Tasks::Task<T>, Args...> {
    using promise_type = typename Tasks::Task<T>::Promise;
};