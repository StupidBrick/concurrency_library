#pragma once

namespace Intrusive {

    struct SinglyDirectedListNode {
        SinglyDirectedListNode* next = nullptr;

        virtual ~SinglyDirectedListNode() = default;
    };

}
