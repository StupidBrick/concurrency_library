#include "fiber.hpp"
#include "fiber_handle.hpp"
#include "iawaiter.hpp"

namespace Fibers {

    thread_local Fiber* current_fiber = nullptr;

    ////FIBER_FUNCTOR
    void FiberFunctor::operator()() {
        current_fiber = fiber_;
        coroutine_->Resume();
        if (!coroutine_->IsCompleted()) {
            (*awaiter_)->AwaitSuspend();
        }
    }

    //// FIBER
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

        // because after suspend we want to reschedule this fiber,
        // and we shouldn't delete this
        step_.need_to_delete_after_discard_ = false;

        coroutine_.Suspend();
    }

    Executors::IExecutor& Fiber::GetScheduler() {
        return *executor_;
    }

    FiberHandle Fiber::Self() {
        return FiberHandle(current_fiber);
    }

    //// FIBER_HANDLE
    void FiberHandle::Schedule() {
        fiber_->Schedule();
    }

    void FiberHandle::YieldSchedule() {
        fiber_->YieldSchedule();
    }

    void FiberHandle::Suspend(Awaiters::IAwaiter* awaiter) {
        fiber_->Suspend(awaiter);
    }

    Executors::IExecutor &FiberHandle::GetScheduler() {
        return fiber_->GetScheduler();
    }

    bool FiberHandle::IsValid() {
        return (fiber_ != nullptr);
    }

}