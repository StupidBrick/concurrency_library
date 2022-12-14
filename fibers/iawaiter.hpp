#pragma once

namespace Fibers::Awaiters {

    class IAwaiter {
    public:
        virtual ~IAwaiter() = default;

        virtual void AwaitSuspend() = 0;
    };

}