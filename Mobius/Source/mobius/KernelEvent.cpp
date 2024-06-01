/**
 * Model for passing a request from the kernel up to the shell for processing
 * optionally with results sent back down.
 *
 * This takes the place of what the old code called ThreadEvent.
 */

#include "../util/Trace.h"

#include "KernelEvent.h"

KernelEvent::KernelEvent()
{
    init();
}

KernelEvent::~KernelEvent()
{
    // Project is the problem child,
    // can we assume to own it if it still exists
    // at this point?  these are enormous and dangerous,
    // let it leak
}

void KernelEvent::init()
{
    next = nullptr;
    type = EventNone;
    returnCode = 0;
    project = nullptr;
    strcpy(arg1, "");
    strcpy(arg2, "");
    strcpy(arg3, "");
    requestId = 0;
}

bool KernelEvent::setArg(int number, const char* value)
{
    bool success = false;

    if (value == nullptr) {
        // why would you bother calling this?
        // clear it if it was already set
        value = "";
    }
    
    if ((strlen(value) + 1) > KernelEventMaxArg) {
        Trace(1, "KernelEvent::setArg argument overflow!\n");
    }
    else {
        char* dest = nullptr;
        switch (number) {
            case 0: dest = arg1; break;
            case 1: dest = arg2; break;
            case 2: dest = arg3; break;
        }
        
        if (dest == nullptr) {
            Trace(1, "KernelEvent::setArg invalid argument number!\n");
        }
        else {
            strcpy(dest, value);
            success = true;
        }
    }
    return success;
}

//////////////////////////////////////////////////////////////////////
// Pool
//////////////////////////////////////////////////////////////////////

/**
 * Pick a comfortable starting size out of the air
 */
const int KernelEventPoolCapacity = 10;

KernelEventPool::KernelEventPool()
{
    mPool = nullptr;
    mAllocated = 0;
    mUsed = 0;
    checkCapacity(KernelEventPoolCapacity);
}

KernelEventPool::~KernelEventPool()
{
    KernelEvent* next = nullptr;
    while (mPool != nullptr) {
        next = mPool->next;
        delete mPool;
        mPool = next;
    }
}

/**
 * Fill the pool with anxious events.
 * Called only during initialization and we're
 * not expected to dynamically grow afterwwards.
 *
 * Obviously NOT thread save.
 */
void KernelEventPool::checkCapacity(int desired)
{
    int count = 0;
    KernelEvent* e = mPool;
    while (e != nullptr) {
        count++;
        e = e->next;
    }
    while (count < desired) {
        e = new KernelEvent();
        e->next = mPool;
        mPool = e;
        count++;
    }
    mAllocated = count;
}

void KernelEventPool::dump()
{
    Trace(2, "KernEventPool: %d events allocated, %d in use\n",
          mAllocated, mUsed);
}

KernelEvent* KernelEventPool::getEvent()
{
    KernelEvent* e = mPool;
    
    if (e == nullptr) {
        Trace(1, "KernEventPool: exhausted!\n");
        e = new KernelEvent();
        mAllocated++;
    }
    else {
        mPool = e->next;
    }
    
    mUsed++;
    if (mUsed > mAllocated)
      Trace(1, "KernelEventPool: In use counter overflow!\n");

    return e;
}

void KernelEventPool::returnEvent(KernelEvent* e)
{
    if (e != nullptr) {
        e->init();
        e->next = mPool;
        mPool = e;
        mUsed--;
        if (mUsed < 0) {
            Trace(1, "KernelEventPool: In use counter underflow!\n");
            mUsed = 0;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
