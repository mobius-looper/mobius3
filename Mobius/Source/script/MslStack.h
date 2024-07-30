/**
 * One frame of the MslSession call stack.
 *
 * This one isn't like the more generic pooled objects like MslValue and MslError.
 * It is used only within MslSession and contains weak references to things
 * like MslScript and MslNode and various runtime state.
 *
 * The only things we own are the childResults and bindings lists.
 * childResults is normally transferred up the stack.  Bindings
 * is reclaimed when this frame finishes.
 */

#pragma once

#include "MslWait.h"

class MslStack
{
  public:
    MslStack();
    ~MslStack();
    void init();

    // script we're in, may not need this?
    class MslScript* script = nullptr;

    // node we're on
    class MslNode* node = nullptr;

    // previous node on the stack
    MslStack* parent = nullptr;

    // a stack frame may have several evaluation phases
    int phase = 0;

    // value(s) for each child node, may be list
    class MslValue* childResults = nullptr;

    // the index of the last child pushed
    // negative means this node has not been started
    int childIndex = -1;

    // true if this frame accumulates all child results
    bool accumulator = false;

    // special state for MslIn iteration
    class MslValue* inList = nullptr;
    class MslValue* inPtr = nullptr;

    // binding list for this block
    class MslBinding* bindings = nullptr;
    void addBinding(class MslBinding* b);

    // phases for complex nodes
    class MslProc* proc = nullptr;
    class MslExternal* external = nullptr;

    // the information we convey to the MslContainer to set up the wait
    // this is only used once so don't need to pool them
    MslWait wait;
};

