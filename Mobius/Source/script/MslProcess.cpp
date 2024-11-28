
#include "../util/Trace.h"

#include "MslObjectPool.h"
#include "MslProcess.h"

MslProcess::MslProcess()
{
}

MslProcess::~MslProcess()
{
    // unclear who owns this
    if (session != nullptr)
      Trace(1, "MslProcess: Destructor leaking session");
    
    if (result != nullptr)
      Trace(1, "MslProcess: Destructor leaking result");
}

void MslProcess::poolInit()
{
    next = nullptr;
    id = 0;
    state = StateNone;
    strcpy(name, "");
    triggerId = 0;
    session = nullptr;
    result = nullptr;
}

void MslProcess::setName(const char* s)
{
    if (s != nullptr)
      strncpy(name, s, sizeof(name));
    else
      strcpy(name, "");
}

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MslProcessPool::MslProcessPool()
{
    setName("MslProcess");
    setObjectSize(sizeof(MslProcess));
    fluff();
}

MslProcessPool::~MslProcessPool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
MslPooledObject* MslProcessPool::alloc()
{
    return new MslProcess();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MslProcess* MslProcessPool::newProcess()
{
    return (MslProcess*)checkout();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
