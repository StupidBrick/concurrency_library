#pragma once

#include "../awaiters.hpp"
#include "../../intrusive/structures/queue.hpp"
#include "../fiber.hpp"

namespace Fibers::Sync {
    class ConditionVariable {
    public:
        template <typename Mutex>
        void Wait(Mutex& mutex) {
            mutex.unlock();
            ::Detail::QueueSpinLock::Guard guard(spinlock_);
            Awaiters::MutexAwaiter awaiter(Fiber::Self(), guard);
            queue_.Push(&awaiter);
            Fiber::Self().Suspend(&awaiter); // here spinlock unlocks
            mutex.lock();
        }

        void NotifyOne() {
            ::Detail::QueueSpinLock::Guard guard(spinlock_);
            if (queue_.Size() > 0) {
                auto* awaiter = (Awaiters::MutexAwaiter*)queue_.TryPop();
                awaiter->Resume();
            }
        }

        void NotifyAll() {
            ::Detail::QueueSpinLock::Guard guard(spinlock_);
            while (queue_.Size() > 0) {
                auto* awaiter = (Awaiters::MutexAwaiter*)queue_.TryPop();
                awaiter->Resume();
            }
        }

    private:
        Detail::QueueSpinLock spinlock_;
        Intrusive::Queue queue_;
    };
}