
#include "../util/Trace.h"

#include "MslObjectPool.h"
#include "MslProcess.h"

MslProcess::MslProcess()
{
}

MslProcess::MslProcess(MslProcess* p)
{
    copy(p);
}

MslProcess::~MslProcess()
{
    // the session is not owned
}

void MslProcess::poolInit()
{
    next = nullptr;
    sessionId = 0;
    state = MslStateNone;
    strcpy(name, "");
    triggerId = 0;
    session = nullptr;
}

void MslProcess::setName(const char* s)
{
    if (s != nullptr)
      strncpy(name, s, sizeof(name));
    else
      strcpy(name, "");
}

void MslProcess::copy(MslProcess* src)
{
    sessionId = src->sessionId;
    state = src->state;
    context = src->context;
    triggerId = src->triggerId;

    // don't really need this
    strncpy(name, src->name, sizeof(name));
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
