#include "thread_pool.hpp"
#include "cassert"

namespace Executors::WithWaitIdle {

    thread_local int thread_id = -1;

    ThreadPool::ThreadPool(size_t workers) : lifo_slots_(workers, nullptr), local_queues_(workers),
                                           lifo_slots_routines_count_(workers, 0) {
        assert(workers > 1);
        for (size_t i = 0; i < workers; ++i) {
            workers_.emplace_back(std::thread([&, this](int j) {
                thread_id = (int)j;

                // wait until all threads are created
                while (!can_start_.test(std::memory_order_acquire)) {
                    can_start_.wait(false, std::memory_order_relaxed);
                }

                WorkerRoutine();
            }, i));
        }

        can_start_.test_and_set(std::memory_order_release);
        can_start_.notify_all();
    }

    ThreadPool::~ThreadPool() {
        assert(workers_.empty() || workers_[0].closed.test(std::memory_order_relaxed));

        // Discard all tasks
        std::lock_guard<std::mutex> guard(global_queue_mutex_);
        Routine* routine;
        while ((routine = (Routine*)global_queue_.TryPop()) != nullptr) {
            if (routine->AllocatedOnHeap()) {
                routine->Discard();
            }
        }

        for (size_t i = 0; i < lifo_slots_.size(); ++i) {
            if (lifo_slots_[i] != nullptr) {
                if (lifo_slots_[i]->AllocatedOnHeap()) {
                    lifo_slots_[i]->Discard();
                }
            }

            while ((routine = (Routine*)local_queues_[i].TryPop()) != nullptr) {
                if (routine->AllocatedOnHeap()) {
                    routine->Discard();
                }
            }
        }
    }

    void ThreadPool::Execute(Routine *routine) {
        Hint hint(Hint::kGlobalQueue);
        if (thread_id != -1) {
            hint.hint = Hint::kLocalQueue;
        }
        Execute(routine, hint);
    }

    void ThreadPool::Execute(Routine *routine, Hint hint) {
        routines_wg_.Add(1);
        bool pushed = false;

        if (hint.hint == Hint::kLocalQueue) {
            PushRoutineInTheLocalQueue(routine, thread_id);
            pushed = true;
        }
        else if (hint.hint == Hint::kGlobalQueue) {
            PushRoutineInTheGlobalQueue(routine);
            pushed = true;
        }
        else if (hint.hint == Hint::kLIFO) {
            PushRoutineInTheLIFOSlot(routine, thread_id);
            pushed = true;
        }

        if (routines_in_queue_.fetch_add(1, std::memory_order_release) == 0) {
            routines_in_queue_.notify_one();
        }

        // if user set wrong hint
        assert(pushed);
    }

    void ThreadPool::YieldExecute(Routine *routine) {
        Execute(routine, Hint(Hint::kGlobalQueue));
    }

    void ThreadPool::WaitIdle() {
        routines_wg_.Wait();
    }

    void ThreadPool::Stop() {
        routines_in_queue_.fetch_add(1'000'000'000'000, std::memory_order_release);
        routines_in_queue_.notify_all();

        // stopped all threads
        for (auto& worker : workers_) {
            worker.closed.test_and_set(std::memory_order_release);
        }

        for (auto& worker : workers_) {
            worker.worker.join();
        }

        routines_wg_.AllDone();
    }

    void ThreadPool::PushRoutineInTheGlobalQueue(Routine* routine) {
        std::lock_guard<std::mutex> guard(global_queue_mutex_);
        global_queue_.Push(routine);
    }

    void ThreadPool::PushRoutineInTheLocalQueue(Routine *routine, size_t queue_id) {
        while (!local_queues_[queue_id].TryPush(routine)) {
            GrabFromLocalQueueToGlobalQueue(queue_id);
        }
    }

    void ThreadPool::PushRoutineInTheLIFOSlot(Routine *routine, size_t slot_id) {
        Routine* lifo = lifo_slots_[slot_id];
        lifo_slots_[slot_id] = routine;
        if (lifo != nullptr) {
            PushRoutineInTheLocalQueue(lifo, slot_id);
        }
    }

    void ThreadPool::GrabFromLocalQueueToGlobalQueue(size_t from) {
        size_t grab_size = kLocalQueueSize / 2;
        Intrusive::Queue grab;
        local_queues_[from].Grab(grab, grab_size);
        if (grab.Size() != 0) {
            std::lock_guard<std::mutex> guard(global_queue_mutex_);
            global_queue_.PushQueue(std::move(grab));
        }
    }

    void ThreadPool::WorkerRoutine() {
        size_t worker_id = thread_id;
        while (!workers_[worker_id].closed.test(std::memory_order_acquire)) {
            Routine* routine;
            if (workers_[worker_id].random_generator() % kGlobalQueueUsingConstant == 0) {
                routine = TryTake(worker_id, TakeStrategy::GetGlobalQueueTakeStrategy());
            }
            else if (lifo_slots_routines_count_[worker_id] >= kMaxLIFORoutinesCount) {
                routine = TryTake(worker_id, TakeStrategy::GetWithoutLIFOSlotTakeStrategy());
            }
            else {
                routine = TryTake(worker_id, TakeStrategy::GetDefaultTakeStrategy());
            }

            if (routine != nullptr) {
                routines_in_queue_.fetch_sub(1, std::memory_order_release);

                // if not allocated on heap,
                // then routine destroys after Run
                // and routine->Discard - UB
                bool need_discard = routine->AllocatedOnHeap();

                routine->Run();
                if (need_discard) {
                    routine->Discard();
                }

                routines_wg_.Done();
                continue;
            }
            else {
                routines_in_queue_.wait(0, std::memory_order_acquire);
            }
        }
    }

    Routine *ThreadPool::TryTake(size_t worker_id, TakeStrategy strategy) {
        Routine* result;
        bool was_local_queue = false;
        for (auto step : strategy.steps) {
            if (step == TakeStrategy::kLIFOSlot) {
                result = TryTakeRoutineFromLIFOSlot(worker_id);
            }
            else if (step == TakeStrategy::kLocalQueue) {
                was_local_queue = true;
                result = TryTakeRoutineFromLocalQueue(worker_id);
            }
            else if (step == TakeStrategy::kGlobalQueue) {
                // because if we served local queue then it is empty
                result = TryTakeRoutineFromGlobalQueue(worker_id, /*grab=*/was_local_queue);
            }
            else if (step == TakeStrategy::kGrab) {
                result = TryGrabRoutineFromLocalQueue(worker_id);
            }

            if (result != nullptr) {
                if (step == TakeStrategy::kLIFOSlot) {
                    ++lifo_slots_routines_count_[worker_id];
                }
                else {
                    lifo_slots_routines_count_[worker_id] = 0;
                }
                return result;
            }
        }

        return nullptr;
    }

    Routine *ThreadPool::TryTakeRoutineFromLIFOSlot(size_t worker_id) {
        Routine* result = lifo_slots_[worker_id];
        lifo_slots_[worker_id] = nullptr;
        return result;
    }

    Routine *ThreadPool::TryTakeRoutineFromLocalQueue(size_t worker_id) {
        return local_queues_[worker_id].TryPop();
    }

    Routine *ThreadPool::TryTakeRoutineFromGlobalQueue(size_t worker_id, bool grab) {
        std::unique_lock<std::mutex> lock(global_queue_mutex_);
        auto* result = (Routine*)global_queue_.TryPop();
        size_t grabbed_count = std::min(kLocalQueueSize / 2, global_queue_.Size() / workers_.size());

        if (result == nullptr || !grab || grabbed_count == 0) {
            return result;
        }

        Intrusive::Queue grabbed;
        Routine* routine;
        for (size_t i = 0; i < grabbed_count && (routine = (Routine*)global_queue_.TryPop()) != nullptr; ++i) {
            grabbed.Push(routine);
        }

        lock.unlock();
        while ((routine = (Routine*)grabbed.TryPop()) != nullptr) {
            PushRoutineInTheLocalQueue(routine, worker_id);
        }

        return result;
    }

    Routine *ThreadPool::TryGrabRoutineFromLocalQueue(size_t to) {
        auto old_robbers_count = robbers_count_.load(std::memory_order_relaxed);
        while (old_robbers_count < workers_.size() && !robbers_count_.compare_exchange_weak(old_robbers_count,
                                                                                            old_robbers_count + 1,
                                                                                            std::memory_order_acq_rel,
                                                                                            std::memory_order_relaxed)) {
        }

        // To many robbers
        if (old_robbers_count >= workers_.size()) {
            return nullptr;
        }

        size_t from;
        while ((from = workers_[to].random_generator() % workers_.size()) == to) {
        }

        Intrusive::Queue grabbed;
        local_queues_[from].Grab(grabbed, kLocalQueueSize / 4);
        auto* result = (Routine*)grabbed.TryPop();
        Routine* routine;
        while ((routine = (Routine*)grabbed.TryPop()) != nullptr) {
            PushRoutineInTheLocalQueue(routine, to);
        }

        return result;
    }

}