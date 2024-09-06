
#include "MslValue.h"
#include "MslError.h"
// temporary until waiting flag can be on the result
#include "MslSession.h"
#include "MslResult.h"

MslResult::MslResult()
{
    init();
}

/**
 * The session pointer is a transient weak reference.
 * Decided not to cascade delete on these because it's more
 * common to delete them independently during result pruning.
 */
MslResult::~MslResult()
{
    delete value;
    delete errors;
}

/**
 * Clean up the object after being brought out of the pool.
 */
void MslResult::init()
{
    next = nullptr;
    sessionId = 0;
    value = nullptr;
    errors = nullptr;
    interned = false;
    session = nullptr;
    strcpy(name, "");
}

MslResult* MslResult::getNext()
{
    return next;
}

bool MslResult::isRunning()
{
    return (session != nullptr);
}

bool MslResult::isWaiting()
{
    return (session != nullptr && session->isWaiting());
}

bool MslResult::isTransitioning()
{
    return (session != nullptr && session->isTransitioning());
}

void MslResult::setName(const char* s)
{
    strncpy(name, s, sizeof(name));
}
    
