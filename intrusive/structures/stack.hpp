#pragma once

#include "singly_directed_list_node.hpp"
#include <iostream>

namespace Intrusive {

    class Stack {
        using Node = SinglyDirectedListNode;
    public:
        Stack() = default;

        Stack(const Stack&) = delete;
        Stack& operator=(const Stack&) = delete;

        Stack(Stack&&) = default;
        Stack& operator=(Stack&&) = default;

        ~Stack() noexcept = default;

        void Push(Node* node) {
            node->next = head_;
            head_ = node;
            ++size_;
        }

        Node* TryPop() {
            if (head_ == nullptr) {
                return nullptr;
            }

            Node* result = head_;
            head_ = head_->next;
            --size_;
            return result;
        }

        void Clear() {
            head_ = nullptr;
            size_ = 0;
        }

        [[nodiscard]] size_t Size() const {
            return size_;
        }

    private:
        Node* head_ = nullptr;
        size_t size_ = 0;
    };

}