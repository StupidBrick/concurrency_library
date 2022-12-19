#pragma once
#include <atomic>

namespace Detail {

    /*  Scalable Queue SpinLock
    *
    *  Usage:
    *
    *  QueueSpinLock qspinlock;
    *  {
    *    QueueSpinLock::Guard lock(qspinlock);  // <-- Acquire
    *    // Critical section
    *  }  // <-- Release
    */

    class QueueSpinLock {
    public:
        class Guard {
        public:
            friend QueueSpinLock;
            explicit Guard(QueueSpinLock& spinlock) : spinlock_(spinlock) {
                spinlock.Acquire(this);
            }

            ~Guard() noexcept {
                if (!released_) {
                    spinlock_.Release(this);
                }
            }

            // twice unlock - UB
            void Unlock() {
                spinlock_.Release(this);
                released_ = true;
            }

        private:
            QueueSpinLock& spinlock_;
            std::atomic<Guard*> next_{ nullptr };
            std::atomic_flag owner_{ false };
            bool released_ = false;
        };

    private:
        void Acquire(Guard* guard) {
            Guard* old_tail_ = tail_.load(std::memory_order_relaxed);
            while (!tail_.compare_exchange_weak(old_tail_, guard,
                                                std::memory_order_acq_rel, // acquire
                                                std::memory_order_relaxed)) {
            }

            if (old_tail_ == nullptr) {
                return;
            }

            old_tail_->next_.store(guard, std::memory_order_release);

            while (!guard->owner_.test(std::memory_order_acquire)) {
                SpinLockPause();
            }
        }

        void Release(Guard* owner) {
            Guard* owner_copy = owner;
            if (tail_.compare_exchange_strong(owner_copy, nullptr,
                                              std::memory_order_release, // release
                                              std::memory_order_relaxed)) {
                return;
            }

            Guard* next;
            while ((next = owner->next_.load(std::memory_order_acquire)) == nullptr) {
                SpinLockPause();
            }

            next->owner_.test_and_set(std::memory_order_release);
        }

        static void SpinLockPause() {
            asm volatile("pause\n" : : : "memory");
        }

    private:
        std::atomic<Guard*> tail_{ nullptr };
    };

}