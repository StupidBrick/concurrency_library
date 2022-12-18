#pragma once

#include "bidirectional_list_node.hpp"
#include <iostream>

namespace Intrusive {

    // single-threaded intrusive queue
    class List {
        using Node = BidirectionalListNode;
    public:
        List() = default;
        List(Node* head, Node* tail, size_t size) : head_(head), tail_(tail), size_(size) {
        }

        List(const List&) = delete;
        List& operator=(const List&) = delete;

        List(List&&) = default;
        List& operator=(List&&) = default;

        ~List() noexcept = default;

        void PushFront(Node* node) {
            if (size_ == 0) {
                size_ = 1;
                node->next = nullptr;
                node->prev = nullptr;
                head_ = node;
                tail_ = node;
                return;
            }

            ++size_;
            node->next = head_;
            node->prev = nullptr;
            head_->prev = node;
            head_ = node;
        }

        void PushBack(Node* node) {
            if (size_ == 0) {
                PushFront(node);
            }

            ++size_;
            node->next = nullptr;
            node->prev = tail_;
            tail_->next = node;
            tail_ = node;
        }

        // return nullptr if queue is empty
        Node* TryPopFront() {
            if (size_ == 0) {
                return nullptr;
            }

            --size_;

            if (size_ == 0) {
                Node* result = head_;
                head_ = nullptr;
                tail_ = nullptr;
                return result;
            }

            Node* result = head_;
            head_ = (Node*)head_->next;
            head_->prev = nullptr;
            return result;
        }

        Node* TryPopBack() {
            if (Size() == 0) {
                return nullptr;
            }

            --size_;

            if (size_ == 0) {
                Node* result = head_;
                head_ = nullptr;
                tail_ = nullptr;
                return result;
            }

            Node* result = tail_;
            tail_ = (Node*)tail_->prev;
            tail_->next = nullptr;
            return result;
        }

        void Unlink(Node* node) {
            if (node == nullptr) {
                return;
            }

            if (node == head_) {
                TryPopFront();
                return;
            }

            if (node == tail_) {
                TryPopBack();
                return;
            }

            auto* prev = (Node*)node->prev;
            auto* next = (Node*)node->next;
            prev->next = next;
            next->prev = prev;
            --size_;
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
