
#include "MslValue.h"
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
    symbol = nullptr;
    wait.reset();
}

