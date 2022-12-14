#pragma once

#include "task_base.hpp"
#include <iostream>
#include <cassert>

namespace Intrusive {

    template <typename Functor>
    class DefaultTask : public TaskBase {
    public:
        DefaultTask(Functor&& func, bool is_allocated_on_heap) : func_(std::forward<Functor>(func)),
                                                                 is_allocated_on_head_(is_allocated_on_heap) {
        }

        bool AllocatedOnHeap() override {
            return is_allocated_on_head_;
        }

        void Run() override {
            func_();
        }

        void Discard() override {
            assert(is_allocated_on_head_);
            delete this;
        }

    private:
        Functor func_;
        const bool is_allocated_on_head_;
    };

}