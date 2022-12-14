#pragma once

#include <functional>
#include <cassert>
#include <exception>
#include <boost/context/continuation_fcontext.hpp>


namespace Coroutines::Stackful {

    using Routine = std::function<void()>;

    class Coroutine {
    public:
        explicit Coroutine(Routine routine) : routine_(std::move(routine)) {
            coroutine_ = boost::context::callcc([this](boost::context::continuation&& sink) {
                suspend_ = &sink;
                Suspend();
                try {
                    routine_();
                }
                catch(...) {
                    exception_ = std::current_exception();
                }

                is_completed_ = true;
                Suspend();

                return std::move(sink);
            });
        }

        Coroutine(const Coroutine&) = delete;
        Coroutine& operator=(const Coroutine&) = delete;

        Coroutine(Coroutine&&) = delete;
        Coroutine& operator=(Coroutine&&) = delete;

        void Resume() {
            coroutine_ = coroutine_.resume();
            if (exception_ != nullptr) {
                std::exception_ptr curr_exception = exception_;
                exception_ = nullptr;
                std::rethrow_exception(curr_exception);
            }
        }

        void Suspend() {
            *suspend_ = suspend_->resume();
        }

        [[nodiscard]] bool IsCompleted() const {
            return is_completed_;
        }

    private:
        boost::context::continuation* suspend_{ nullptr };

        boost::context::continuation coroutine_;

        bool is_completed_{ false };
        Routine routine_;

        std::exception_ptr exception_{ nullptr };
    };

}
