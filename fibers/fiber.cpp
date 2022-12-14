#include "fiber.hpp"
#include "fiber_handle.hpp"
#include "iawaiter.hpp"


namespace Fibers {

    using Executors::IExecutor;

    thread_local Fiber* current_fiber = nullptr;

    Fiber::Fiber(std::function<void()> routine,
                 Executors::IExecutor& executor) : coroutine_(std::move(routine)), executor_(&executor) {
    }

    void Fiber::Schedule() {
        executor_->Execute(&step_);
    }

    void Fiber::YieldSchedule() {
        executor_->YieldExecute(&step_);
    }

    void Fiber::Suspend(Awaiters::IAwaiter* awaiter) {
        awaiter_ = awaiter;
        coroutine_.Suspend();
    }

    void Fiber::Resume() {
        current_fiber = this;
        coroutine_.Resume();
        if (coroutine_.IsCompleted()) {
            delete this;
        }
        else {
            awaiter_->AwaitSuspend();
        }
    }

    Executors::IExecutor& Fiber::GetScheduler() {
        return *executor_;
    }

    FiberHandle Fiber::Self() {
        return FiberHandle(current_fiber);
    }

    void Fiber::Functor_::operator()() {
        current_fiber = fiber_;
        coroutine_->Resume();
        if (coroutine_->IsCompleted()) {
            delete fiber_;
        }
        else {
            (*awaiter_)->AwaitSuspend();
        }
    }

    void FiberHandle::Schedule() {
        fiber_->Schedule();
    }

    void FiberHandle::YieldSchedule() {
        fiber_->YieldSchedule();
    }

    void FiberHandle::Suspend(Awaiters::IAwaiter* awaiter) {
        fiber_->Suspend(awaiter);
    }

    void FiberHandle::Resume() {
        fiber_->Resume();
    }

    Executors::IExecutor &FiberHandle::GetScheduler() {
        return fiber_->GetScheduler();
    }

    bool FiberHandle::IsValid() {
        return (fiber_ != nullptr);
    }

}