#pragma once

#include "../fiber.hpp"
#include "../awaiters.hpp"
#include "../../detail/spinlock.hpp"
#include "../../intrusive/structures/queue.hpp"

namespace Fibers::Sync {

    class Mutex {
    public:
        void Lock() {
            Detail::QueueSpinLock::Guard guard(spinlock_);
            if (!closed_) {
                closed_ = true;
                return;
            }

            Awaiters::MutexAwaiter awaiter(Fiber::Self(), guard);
            queue_.Push(&awaiter);
            Fiber::Self().Suspend(&awaiter);
        }

        void Unlock() {
            Detail::QueueSpinLock::Guard guard(spinlock_);

            if (queue_.Size() > 0) {
                auto* awaiter = (Awaiters::MutexAwaiter*)queue_.TryPop();
                awaiter->Resume();
            }
            else {
                closed_ = false;
            }
        }

        // basic lockable
        void lock() {
            Lock();
        }

        void unlock() {
            Unlock();
        }

    private:
        Detail::QueueSpinLock spinlock_;
        bool closed_ = false;
        Intrusive::Queue queue_;
    };

}