#pragma once

namespace Intrusive {

    class ITask {
    public:
        virtual bool AllocatedOnHeap() = 0;

        virtual ~ITask() = default;
        virtual void Run() = 0;
        virtual void Discard() = 0;
    };

}