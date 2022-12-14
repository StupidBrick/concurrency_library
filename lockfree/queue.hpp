#pragma once

#include <atomic>
#include <optional>
#include <memory>


namespace LockFree {

    // Michael-Scott Queue
    template <typename T>
    class Queue {
    private:
        template <typename U>
        struct CountedPtr {
        private:
            static const int kPointerSize = 48;
            static const uint64_t kPointerMask = 0x0000ffffffffffff;
            static const uint64_t kCounterMask = 0xffff000000000000;
            uint64_t dirty_ptr_ = 0;

        public:
            CountedPtr() = default;

            explicit CountedPtr(U* ptr) : dirty_ptr_((uint64_t)ptr) {
            }

            CountedPtr(U* ptr, uint64_t count) : dirty_ptr_((uint64_t)ptr + (count << kPointerSize)) {
            }

            U* GetPointer() const {
                return (U*)(dirty_ptr_ & kPointerMask);
            }

            void SetPointer(U* ptr) {
                dirty_ptr_ = (dirty_ptr_ & kCounterMask) + (uint64_t)ptr;
            }

            U* operator->() {
                return GetPointer();
            }

            U& operator*() {
                return *GetPointer();
            }

            const U* operator->() const {
                return GetPointer();
            }

            const U& operator*() const {
                return *GetPointer();
            }

            [[nodiscard]] uint64_t GetCount() const {
                return (dirty_ptr_ >> kPointerSize);
            }

            void IncreaseCount() {
                dirty_ptr_ += ((uint64_t)1 << kPointerSize);
            }

        };

        struct Node {
            static const long long kOneCounter = ((uint64_t)1 << 60);

            T* value;
            std::atomic<long long> internal_count;
            std::atomic<CountedPtr<Node>> next;

            explicit Node(T* value) : value(value), internal_count(2 * kOneCounter) {
            }

            Node() : value(nullptr), internal_count(2 * kOneCounter) {
            }
        };

    public:
        Queue();

        void Push(T&& value);

        std::optional<T> TryPop();

        void Clear();

        ~Queue() noexcept;

    private:
        static void IncreaseExternalCount(std::atomic<CountedPtr<Node>>& atomic_ptr, CountedPtr<Node>& old_atomic);

        static void ReleaseRef(Node* node);

        static void ReleaseOneCounter(CountedPtr<Node>& ptr, int inc = 0);

    private:
        std::atomic<CountedPtr<Node>> head_;
        std::atomic<CountedPtr<Node>> tail_;
    };

    template <typename T>
    Queue<T>::Queue() {
        CountedPtr<Node> head(new Node, 1);
        head_.store(head, std::memory_order_relaxed);
        tail_.store(head, std::memory_order_relaxed);
    }

    template <typename T>
    void Queue<T>::Clear() {
        while (TryPop() != std::nullopt) {
        }
    }

    template <typename T>
    void Queue<T>::Push(T &&value) {
        std::unique_ptr<T> ptr = std::make_unique<T>(std::forward<T>(value));
        CountedPtr<Node> new_tail(new Node(ptr.get()), 1);
        ptr.release();

        CountedPtr<Node> old_tail = tail_.load(std::memory_order_relaxed);
        while (true) {
            IncreaseExternalCount(tail_, old_tail);
            CountedPtr<Node> old_tail_ptr;

            if (old_tail->next.compare_exchange_strong(old_tail_ptr, new_tail)) { // TODO memory order
                if (tail_.compare_exchange_strong(old_tail, new_tail)) { // TODO memory order
                    ReleaseOneCounter(old_tail);
                }
                return;
            }

            Node* node_ptr = old_tail.GetPointer();
            if (tail_.compare_exchange_strong(old_tail, old_tail->next.load())) { // TODO memory order
                ReleaseOneCounter(old_tail);
            }
            else {
                ReleaseRef(node_ptr);
            }
        }
    }

    template <typename T>
    std::optional<T> Queue<T>::TryPop() {
        CountedPtr<Node> old_head = head_.load(std::memory_order_relaxed);
        CountedPtr<Node> next_head = head_.load(std::memory_order_relaxed);
        while (true) {
            IncreaseExternalCount(head_, old_head);
            IncreaseExternalCount(old_head->next, next_head);
            Node* old_head_ptr = old_head.GetPointer();

            CountedPtr<Node> tail = tail_.load(); // TODO memory order

            if (old_head_ptr == tail.GetPointer()) {
                if (next_head.GetPointer() == nullptr) {
                    return std::nullopt;
                }

                if (tail_.compare_exchange_strong(tail, next_head)) { // TODO memory order
                    ReleaseOneCounter(tail, 1); // 1 : because we didn't capture tail
                }
            }
            else if (head_.compare_exchange_strong(old_head, next_head)) { // TODO memory order
                T* result_ptr = std::move(next_head->value);
                ReleaseOneCounter(old_head);
                ReleaseRef(next_head.GetPointer());

                T result = std::move(*result_ptr);
                delete result_ptr;

                return { std::move(result) };
            }

            ReleaseRef(old_head_ptr);
            ReleaseRef(next_head.GetPointer());
        }
    }

    template <typename T>
    void Queue<T>::ReleaseOneCounter(CountedPtr <Node> &ptr, int inc) {
        long long increase = (long long)ptr.GetCount() - 2 - Node::kOneCounter + inc;

        if (ptr->internal_count.fetch_add(increase) == -increase) {
            delete ptr.GetPointer();
        }
    }

    template <typename T>
    void Queue<T>::IncreaseExternalCount(std::atomic<CountedPtr<Node>> &atomic_ptr, CountedPtr <Node> &old_atomic) {
        CountedPtr<Node> new_atomic;
        do {
            new_atomic = old_atomic;
            new_atomic.IncreaseCount();
        } while (!atomic_ptr.compare_exchange_strong(old_atomic, new_atomic));
        old_atomic.IncreaseCount();
    }

    template <typename T>
    void Queue<T>::ReleaseRef(Node *node) {
        if (node->internal_count.fetch_sub(1) == 1) {
            delete node;
        }
    }

    template <typename T>
    Queue<T>::~Queue() noexcept{
        CountedPtr<Node> head = head_.load(std::memory_order_relaxed);

        while (head.GetPointer() != nullptr) {
            CountedPtr<Node> new_head = head->next.load(std::memory_order_relaxed);
            delete head.GetPointer();
            head = new_head;
        }
    }


}