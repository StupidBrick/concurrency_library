#pragma once

#include "../../iexecutor.hpp"

#include "../../../intrusive/tasks/default_task.hpp"
#include "../../../intrusive/structures/queue.hpp"
#include "../../../intrusive/structures/stack.hpp"
#include <thread>
#include <vector>

#include "../../../lockfree/ring_queue.hpp"

#include <mutex>

#include <random>


namespace Executors::WithoutWaitIdle {

    struct Hint {
        // hint to push routine in the LIFO slot
        const static uint8_t kLIFO = 0;

        // hint to push routine in the local queue
        const static uint8_t kLocalQueue = 1;

        // hint to push task into the global queue
        const static uint8_t kGlobalQueue = 2;

        uint8_t hint;

        explicit Hint(uint8_t hint) : hint(hint) {
        }
    };

    class ThreadPool : public IExecutor {
    private:
        // alignas(64) to avoid extra cache synchronizations
        struct alignas(64) Worker {
            std::thread worker;

            std::atomic_flag closed{ false };
            std::random_device device;
            std::mt19937 random_generator{ device() };

            explicit Worker(std::thread&& thread) : worker(std::move(thread)) {
            }

            Worker(Worker&& other) noexcept {
                worker = std::move(other.worker);
            }

            Worker& operator=(Worker&& other) noexcept {
                worker = std::move(other.worker);
                return *this;
            }
        };

        struct TakeStrategy {
            static const uint8_t kLIFOSlot = 0;
            static const uint8_t kLocalQueue = 1;
            static const uint8_t kGlobalQueue = 2;
            static const uint8_t kGrab = 3;

            uint8_t steps[4];

            TakeStrategy(uint8_t step1, uint8_t step2, uint8_t step3, uint8_t step4) : steps{ step1, step2,
                                                                                              step3, step4 } {
            }

            static TakeStrategy GetDefaultTakeStrategy() {
                return { kLIFOSlot, kLocalQueue, kGlobalQueue, kGrab };
            }

            static TakeStrategy GetGlobalQueueTakeStrategy() {
                return { kGlobalQueue, kLIFOSlot, kLocalQueue, kGrab };
            }

            static TakeStrategy GetWithoutLIFOSlotTakeStrategy() {
                return { kLocalQueue, kGlobalQueue, kGrab, kLIFOSlot };
            }
        };

    public:
        explicit ThreadPool(size_t workers);

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        void Execute(Routine* routine) override;

        void Execute(Routine* routine, Hint hint);

        void YieldExecute(Routine* routine) override;

        void Stop();

        ~ThreadPool() override;

    private:
        void WorkerRoutine();

        // Execute routines
        void PushRoutineInTheGlobalQueue(Routine* routine);
        void PushRoutineInTheLocalQueue(Routine* routine, size_t queue_id);
        void PushRoutineInTheLIFOSlot(Routine* routine, size_t slot_id);
        void GrabFromLocalQueueToGlobalQueue(size_t from);

        // Take routines
        Routine* TryTake(size_t worker_id, TakeStrategy strategy);
        Routine* TryTakeRoutineFromLIFOSlot(size_t worker_id);
        Routine* TryTakeRoutineFromLocalQueue(size_t worker_id);
        Routine* TryTakeRoutineFromGlobalQueue(size_t worker_id, bool grab = true);
        Routine* TryGrabRoutineFromLocalQueue(size_t to);

    private:
        constexpr static size_t kLocalQueueSize = 1024;
        constexpr static size_t kMaxLIFORoutinesCount = 20;

        // if rand() % kGlobalQueueUsingConstant == 0
        // we take routine from the queue bypassing the LIFO slot
        // it needs to complete all tasks from the global queue
        constexpr static size_t kGlobalQueueUsingConstant = 61;

        std::vector<Worker> workers_;
        std::vector<Routine*> lifo_slots_;

        std::vector<LockFree::RingQueue<Routine, kLocalQueueSize>> local_queues_;

        std::mutex global_queue_mutex_;
        Intrusive::Queue global_queue_; // guarded by global_queue_mutex_

        // we must maintain the invariant
        // robbers_count_ <= workers_.size() / 2
        std::atomic<size_t> robbers_count_{ 0 };

        std::atomic_flag can_start_{ false };

        // counts how many routines were launched in a row through the lifo slot
        // if >= 20, then the next routine will not start through the lifo slot
        std::vector<size_t> lifo_slots_routines_count_;
    };

}