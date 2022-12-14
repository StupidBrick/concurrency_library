#pragma once

#include "itask.hpp"
#include "../structures/singly_directed_list_node.hpp"

namespace Intrusive {

    class TaskBase : public ITask, public SinglyDirectedListNode {
    };

}