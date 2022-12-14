#pragma once

#include <atomic>
#include <optional>


namespace LockFree {

    // memory control idea :
    // we have 2 counters : external (CountedPtr<Node>.GetCount()) and internal (Node.internal_count)
    // external counts number of all views
    // internal counts number of all stopping views
    // when internal + external == 0 we can delete node
    template <typename T>
    class Stack {
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

        struct Node {
            T value;
            std::atomic<long long> internal_count{ 0 };
            CountedPtr<Node> next;

            explicit Node(T value) : value(std::move(value)) {
            }
        };

    public:
        void Push(T&& value);

        std::optional<T> TryPop();

        void Clear();

        ~Stack() noexcept;

    private:
        void IncreaseExternalCount(CountedPtr<Node>& old_head);

    private:
        std::atomic<CountedPtr<Node>> head_;
    };

    template <typename T>
    void Stack<T>::Push(T&& value) {
        auto* node = new Node(std::forward<T>(value));
        CountedPtr<Node> new_head(node, 1);
        new_head->next = head_.load(std::memory_order_relaxed);

        while (!head_.compare_exchange_weak(new_head->next, new_head, std::memory_order_release,
                                            std::memory_order_relaxed)) {
        }
    }

    template <typename T>
    std::optional<T> Stack<T>::TryPop() {
        CountedPtr<Node> old_head = head_.load(std::memory_order_relaxed);

        while (true) {
            IncreaseExternalCount(old_head);

            if (old_head.GetPointer() == nullptr) {
                return std::nullopt;
            }

            Node* old_head_ptr = old_head.GetPointer();
            if (head_.compare_exchange_strong(old_head, old_head->next, std::memory_order_acq_rel,
                                              std::memory_order_relaxed)) {
                std::optional<T> result = std::move(old_head_ptr->value);

                long long add = (long long)old_head.GetCount() - 2;

                if (old_head_ptr->internal_count.fetch_add(add, std::memory_order_acq_rel) == -add) {
                    delete old_head_ptr;
                }

                return std::move(result);
            }

            if (old_head_ptr->internal_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                delete old_head_ptr;
            }
        }
    }

    template <typename T>
    void Stack<T>::Clear() {
        while (TryPop() != std::nullopt) {
        }
    }

    template <typename T>
    Stack<T>::~Stack() noexcept {
        CountedPtr<Node> head = head_.load(std::memory_order_relaxed);
        while (head.GetPointer() != nullptr) {
            CountedPtr<Node> next = head->next;
            delete head.GetPointer();
            head = next;
        }
    }

    template <typename T>
    void Stack<T>::IncreaseExternalCount(CountedPtr<Node> &old_head) {
        CountedPtr<Node> old_head_copy;
        do {
            old_head_copy = old_head;
            old_head_copy.IncreaseCount();
        } while (!head_.compare_exchange_strong(old_head, old_head_copy, std::memory_order_acquire,
                                                std::memory_order_relaxed));
        old_head.IncreaseCount();
    }

}