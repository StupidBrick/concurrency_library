cmake_minimum_required(VERSION 3.23)
project(concurrency_library)

set (CMAKE_CXX_STANDARD 20)

FIND_PACKAGE(Boost COMPONENTS context REQUIRED)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

set(PROJECT_HEADERS
        lockfree/ring_queue.hpp
        lockfree/stack.hpp
        coroutines/stackful/coroutine.hpp
        detail/spinlock.hpp
        intrusive/tasks/default_task.hpp
        intrusive/tasks/itask.hpp
        intrusive/tasks/task_base.hpp
        intrusive/structures/singly_directed_list_node.hpp
        intrusive/structures/stack.hpp
        intrusive/structures/queue.hpp
        executors/iexecutor.hpp
        executors/api.hpp
        executors/manual_executor.hpp
        executors/thread_pool/with_waitidle/thread_pool.hpp
        detail/waitgroup.hpp
        futures/result.hpp
        futures/detail/type_traits.hpp
        futures/future_value.hpp
        futures/future.hpp
        futures/promise.hpp
        futures/api/future.hpp
        futures/api/all.hpp
        futures/api/execute.hpp
        futures/api/first_of.hpp
        futures/callbacks/icallback.hpp
        futures/callbacks/subscribe_callback.hpp
        futures/callbacks/then_callback.hpp
        futures/callbacks/recover_callback.hpp
        executors/thread_pool/without_waitidle/thread_pool.hpp
        fibers/awaiters.hpp
        executors/strand.hpp
        fibers/fiber.hpp
        fibers/api.hpp
        fibers/fiber_handle.hpp
        lockfree/queue.hpp
        coroutines/stackless/task.hpp
        fibers/sync/mutex.hpp
        fibers/iawaiter.hpp
        fibers/sync/waitgroup.hpp
        fibers/sync/condition_variable.hpp
        channels/channel.hpp channels/select.hpp
        intrusive/structures/bidirectional_list_node.hpp
        intrusive/structures/list.hpp)

set (PROJECT_SOURCES
        main.cpp
        executors/thread_pool/with_waitidle/thread_pool.cpp
        executors/thread_pool/without_waitidle/thread_pool.cpp
        fibers/fiber.cpp lockfree/atomic_shared_ptr.hpp)

add_executable(ConcurrencyLibrary ${PROJECT_SOURCES} ${PROJECT_HEADERS})

TARGET_LINK_LIBRARIES(ConcurrencyLibrary LINK_PUBLIC ${Boost_LIBRARIES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
