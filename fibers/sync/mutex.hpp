#pragma once

#include "../fiber.hpp"
#include "../awaiters.hpp"

namespace Fibers::Sync {

    class Mutex {
    public:
        void Lock() {

        }

        void Unlock() {

        }

        // basic lockable
        void lock() {
            Lock();
        }

        void unlock() {
            Unlock();
        }
    };

}