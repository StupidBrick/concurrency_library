#pragma once

#include "singly_directed_list_node.hpp"

namespace Intrusive {

    struct BidirectionalListNode : public SinglyDirectedListNode {
        SinglyDirectedListNode* prev = nullptr;
    };

}
