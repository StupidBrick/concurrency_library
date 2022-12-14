#pragma once

#include "singly_directed_list_node.hpp"
#include <iostream>

namespace Intrusive {

    // single-threaded intrusive queue
    class Queue {
        using Node = SinglyDirectedListNode;
    public:
        Queue() = default;
        Queue(Node* head, Node* tail, size_t size) : head_(head), tail_(tail), size_(size) {
        }

        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;

        Queue(Queue&&) = default;
        Queue& operator=(Queue&&) = default;

        ~Queue() noexcept = default;

        void Push(Node* node) {
            if (node == nullptr) {
                return;
            }

            node->next = nullptr;

            if (head_ == nullptr) {
                head_ = node;
                tail_ = node;
            }
            else {
                tail_->next = node;
                tail_ = node;
            }
            ++size_;
        }

        // merge 2 queues
        void PushQueue(Queue&& queue) {
            if (queue.Size() == 0) {
                return;
            }

            if (Size() == 0) {
                head_ = queue.head_;
                tail_ = queue.tail_;
                size_ = queue.size_;
            }
            else {
                tail_->next = queue.head_;
                tail_ = queue.tail_;
                size_ += queue.size_;
            }

            queue.head_ = nullptr;
            queue.tail_ = nullptr;
            queue.size_ = 0;
        }

        // return nullptr if queue is empty
        Node* TryPop() {
            if (head_ == nullptr) {
                return nullptr;
            }

            Node* result = head_;
            --size_;

            if (head_ == tail_) {
                head_ = nullptr;
                tail_ = nullptr;
            }
            else {
                head_ = head_->next;
            }

            return result;
        }

        void Clear() {
            head_ = nullptr;
            tail_ = nullptr;
            size_ = 0;
        }

        [[nodiscard]] size_t Size() const {
            return size_;
        }

    private:
        Node* head_ = nullptr;
        Node* tail_ = nullptr;
        size_t size_ = 0;
    };

}
