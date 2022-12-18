#pragma once

#include <functional>
#include <cassert>
#include <exception>
#include <boost/context/continuation_fcontext.hpp>


namespace Coroutines::Stackful {

    using Routine = std::function<void()>;

    using Routine = std::function<void()>;

    class Coroutine {
    public:
        explicit Coroutine(Routine routine) : routine_(std::move(routine)) {
            coroutine_ = boost::context::callcc([this, routine = routine_](boost::context::continuation&& sink) {
                suspend_ = &sink;
                Suspend();
                is_started_ = true;
                try {
                    routine();
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
            coroutine_ = std::move(coroutine_.resume());
            if (exception_ != nullptr) {
                std::exception_ptr curr_exception = exception_;
                exception_ = nullptr;
                std::rethrow_exception(curr_exception);
            }
        }

        void Suspend() {
            *suspend_ = std::move(suspend_->resume());
            if (exception_ != nullptr) {
                std::rethrow_exception(exception_);
            }
        }

        [[nodiscard]] bool IsCompleted() const {
            return is_completed_;
        }

        ~Coroutine() {
            // in this case default destructor throw assertion error
            if (is_started_ && !is_completed_) {
                exception_ = std::make_exception_ptr(std::exception());
                try {
                    Resume();
                } catch(...) {}
            }
        }

    private:
        boost::context::continuation coroutine_;
        boost::context::continuation* suspend_{ nullptr };

        bool is_completed_{ false };
        bool is_started_{ false };
        Routine routine_;

        std::exception_ptr exception_{ nullptr };
    };

}
