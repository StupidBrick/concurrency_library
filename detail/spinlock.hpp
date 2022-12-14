#pragma once
#include <atomic>

namespace Detail {

    /*  Scalable Queue SpinLock
    *
    *  Usage:
    *
    *  QueueSpinLock qspinlock;
    *  {
    *    QueueSpinLock::UniqueLock lock(qspinlock);  // <-- Acquire
    *    // Critical section
    *  }  // <-- Release
    */

    class QueueSpinLock {
    public:
        class UniqueLock {
        public:
            friend QueueSpinLock;
            explicit UniqueLock(QueueSpinLock& spinlock) : spinlock_(spinlock) {
                spinlock.Acquire(this);
            }

            void Lock() {
                locked_ = true;
                spinlock_.Acquire(this);
            }

            void Unlock() {
                locked_ = false;
                spinlock_.Release(this);
            }

            ~UniqueLock() noexcept {
                if (locked_) {
                    spinlock_.Release(this);
                }
            }

        private:
            QueueSpinLock& spinlock_;
            bool locked_ = true;
            std::atomic<UniqueLock*> next_ = nullptr;
            std::atomic_flag owner_{ false };
        };

    private:
        void Acquire(UniqueLock* guard) {
            UniqueLock* old_tail = tail_.load(std::memory_order_relaxed);
            while (!tail_.compare_exchange_weak(old_tail, guard,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
            }
            if (old_tail != nullptr) {
                old_tail->next_.store(guard, std::memory_order_release);
                while (!guard->owner_.test(std::memory_order_acquire)) {
                    SpinLockPause();
                }
            }
        }

        void Release(UniqueLock* owner) {
            UniqueLock* owner_copy = owner;
            if (!tail_.compare_exchange_strong(owner_copy, nullptr,
                                               std::memory_order_release,
                                               std::memory_order_relaxed)) {
                UniqueLock* next;
                while ((next = owner->next_.load(std::memory_order_acquire)) == nullptr) {
                    SpinLockPause();
                }
                next->owner_.test_and_set(std::memory_order_release);
            }
        }

        static void SpinLockPause() {
            asm volatile("pause\n" : : : "memory");
        }

    private:
        std::atomic<UniqueLock*> tail_{ nullptr };
    };

}