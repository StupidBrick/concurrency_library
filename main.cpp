#include "executors/thread_pool/with_waitidle/thread_pool.hpp"
#include "fibers/api.hpp"
#include "fibers/sync/mutex.hpp"
#include "fibers/sync/waitgroup.hpp"
#include <mutex>
#include <condition_variable>


int main() {
    Executors::WithWaitIdle::ThreadPool tp{ 4 };

    Fibers::Sync::Mutex mutex;
    std::condition_variable cv;

    Fibers::Go(tp, [&mutex, &cv]() {
        std::unique_lock<Fibers::Sync::Mutex> lock(mutex);
        cv.wait(lock);
    });


    tp.WaitIdle();
    tp.Stop();
}