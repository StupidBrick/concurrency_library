#pragma once

#include <atomic>
#include <optional>
#include <memory>
#include <map>
#include <vector>


namespace LockFree {

    // Michael-Scott Queue

    // memory control idea :
    // we have 3 counters : external (CountedPtr<Node>.GetCount()), internal (first 32 bits of Node.internal_count)
    // and count of external counters (others bits of Node.internal_count)
    // external counts number of this atomic counted ptr viewers
    // internal counts number of all stopping views
    // count of external counters <= 2 ((previous node next or head_) and tail_)
    // when internal + all externals == 0 we can delete node
    template <typename T>
    class Queue {
    private:
        // pointer + counter
        // first 16 bit - counter
        // last 48 bits - pointer
        // structure takes 64 bits to fit in 1 machine word
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

        // internal_count == -count of all stopping views + count of external counters * kOneCounter
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
        void IncreaseExternalCount(std::atomic<CountedPtr<Node>>& atomic_ptr, CountedPtr<Node>& old_atomic);

        void ReleaseRef(Node* node);

        void ReleaseOneCounter(CountedPtr<Node>& ptr, long long inc = 0);

    private:
        std::atomic<CountedPtr<Node>> head_;
        std::atomic<CountedPtr<Node>> tail_;
    };

    template <typename T>
    Queue<T>::Queue() {
        Node* head = new Node(nullptr);
        head_.store(CountedPtr<Node>(head, 1), std::memory_order_relaxed);
        tail_.store(CountedPtr<Node>(head, 1), std::memory_order_relaxed);
    }

    template <typename T>
    Queue<T>::~Queue() noexcept {
        CountedPtr<Node> head = head_.load(std::memory_order_relaxed);
        CountedPtr<Node> next = head->next.load(std::memory_order_relaxed);
        delete head.GetPointer();
        head = next;
        while (head.GetPointer() != nullptr) {
            delete head->value;
            next = head->next.load(std::memory_order_relaxed);
            delete head.GetPointer();
            head = next;
        }
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

        CountedPtr<Node> old_tail = tail_.load();
        while (true) {
            IncreaseExternalCount(tail_, old_tail);

            CountedPtr<Node> old_tail_copy = old_tail;

            CountedPtr<Node> old_tail_next; // nullptr
            if (old_tail->next.compare_exchange_strong(old_tail_next, new_tail)) {
                if (tail_.compare_exchange_strong(old_tail_copy, new_tail)) {
                    ReleaseOneCounter(old_tail);
                }
                else {
                    ReleaseRef(old_tail.GetPointer());
                }
                return;
            }
            else {
                // helps to another thread
                CountedPtr<Node> next_tail = old_tail->next.load();
                if (next_tail.GetPointer() != nullptr  && tail_.compare_exchange_strong(old_tail_copy, next_tail)) {
                    ReleaseOneCounter(old_tail);
                }
                else {
                    ReleaseRef(old_tail.GetPointer());
                }
            }
        }
    }

    template <typename T>
    std::optional<T> Queue<T>::TryPop() {
        CountedPtr<Node> head = head_.load();
        while (true) {
            IncreaseExternalCount(head_, head);
            CountedPtr<Node> tail = tail_.load();
            CountedPtr<Node> next_head = head->next.load();

            if (head.GetPointer() == tail.GetPointer()) {
                if (next_head.GetPointer() == nullptr) {
                    ReleaseRef(head.GetPointer());
                    return std::nullopt;
                }
                // helps to another thread
                if (tail_.compare_exchange_strong(tail, next_head))  {
                    ReleaseOneCounter(tail, 1); // +1 because we didn't increase the external counter
                }
            }
            else {
                next_head.IncreaseCount(); // increase external counter

                CountedPtr<Node> head_copy = head;
                if (head_.compare_exchange_strong(head_copy, next_head)) {
                    T result = std::move(*next_head->value);
                    delete next_head->value;
                    ReleaseOneCounter(head);
                    ReleaseRef(next_head.GetPointer());
                    return std::move(result);
                }
            }

            ReleaseRef(head.GetPointer());
        }
    }

    template <typename T>
    void Queue<T>::IncreaseExternalCount(std::atomic<CountedPtr<Node>> &atomic_ptr, CountedPtr<Node> &old_atomic) {
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
    void Queue<T>::ReleaseOneCounter(CountedPtr<Node> &ptr, long long inc) {
        long long increase = (long long)ptr.GetCount() - 2 - Node::kOneCounter + inc;
        if (ptr->internal_count.fetch_add(increase) == -increase) {
            delete ptr.GetPointer();
        }
    }

}