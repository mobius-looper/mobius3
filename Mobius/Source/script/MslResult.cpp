
#include "MslValue.h"
#include "MslResult.h"

MslResult::MslResult()
{
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
}

MslResult* MslResult::getNext()
{
    return next;
}

bool MslResult::isRunning()
{
    return (session != nullptr);
}

