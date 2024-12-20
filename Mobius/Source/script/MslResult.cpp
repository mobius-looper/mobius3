
#include "MslValue.h"
#include "MslError.h"
#include "MslResult.h"
#include "MslEnvironment.h"

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
    delete results;
}

/**
 * Clean up the object after being brought out of the pool.
 */
void MslResult::init()
{
    next = nullptr;
    sessionId = 0;
    state = MslStateNone;
    value = nullptr;
    errors = nullptr;
    results = nullptr;
    //interned = false;
    strcpy(name, "");
}

MslResult* MslResult::getNext()
{
    return next;
}

void MslResult::setName(const char* s)
{
    strncpy(name, s, sizeof(name));
}

//////////////////////////////////////////////////////////////////////
//
// Builder
//
//////////////////////////////////////////////////////////////////////

/**
 * The different builder constructors determine whether we use
 * the environment's object pools to create objects, and whether
 * we create a new result vs. modify an existing result.
 */
MslResultBuilder::MslResultBuilder()
{
}

MslResultBuilder::MslResultBuilder(MslEnvironment* env)
{
    environment = env;
}

MslResultBuilder::MslResultBuilder(MslResult* r)
{
    result = r;
    externalResult = true;
}

MslResultBuilder::MslResultBuilder(MslEnvironment* env, MslResult* r)
{
    environment = env;
    result = r;
    externalResult = true;
}

/**
 * Normally will have called finish() to transfer the assembed result
 * but delete it if it was left behind.
 */
MslResultBuilder::~MslResultBuilder()
{
    if (!externalResult)
      delete result;
}

MslResult* MslResultBuilder::finish()
{
    // ensure this?
    MslResult* r = result;
    result = nullptr;
    return r;
}

void MslResultBuilder::addError(const char* msg)
{
    MslError* error = allocError();
    // todo: if thsi can be used by the containing appication
    // will want more options for this
    error->source = MslSourceEnvironment;
    error->setDetails(msg);
    
    ensureResult();
    error->next = result->errors;
    result->errors = error;
}

MslError* MslResultBuilder::allocError()
{
    MslError* error;
    if (environment == nullptr)
      error = new MslError();
    else
      error = environment->getPool()->allocError();
    return error;
}

void MslResultBuilder::ensureResult()
{
    if (result == nullptr) {
        if (environment == nullptr)
          result = new MslResult();
        else
          result = environment->getPool()->allocResult();
        externalResult = false;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
