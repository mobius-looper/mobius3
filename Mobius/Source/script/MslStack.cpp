
#include "MslValue.h"
#include "MslBinding.h"
#include "MslStack.h"

MslStack::MslStack()
{
}

MslStack::~MslStack()
{
    delete childResults;
    delete bindings;
}

void MslStack::init()
{
    script = nullptr;
    node = nullptr;
    parent = nullptr;
    phase = 0;
    childResults = nullptr;
    childIndex = -1;
    accumulator = false;
    bindings = nullptr;
    proc = nullptr;
    external = nullptr;
    wait.init();
}

/**
 * Keep bindings ordered, not sure if necessary
 */
void MslStack::addBinding(MslBinding* b)
{
    if (bindings == nullptr)
      bindings = b;
    else {
        MslBinding* last = bindings;
        while (last->next != nullptr)
          last = last->next;
        last->next = b;
    }
}
