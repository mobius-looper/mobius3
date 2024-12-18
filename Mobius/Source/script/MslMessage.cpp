
#include "../util/Trace.h"

#include "MslObjectPool.h"
#include "MslConstants.h"
#include "MslBinding.h"
#include "MslValue.h"

#include "MslMessage.h"

MslMessage::MslMessage()
{
}

MslMessage::~MslMessage()
{
    // this cascades
    delete request.bindings;
    // so does this
    delete request.arguments;

    // session I'm not sure about ownership
    if (session != nullptr)
      Trace(1, "MslMessage: Leaking session");

    // these are probably safe to delete, if they were caught
    // in transition during shutdown no one owns them
    if (result != nullptr)
      Trace(1, "MslMessage: Leaking result");

    // the message itself itself does not cascade
}

void MslMessage::poolInit()
{
    next = nullptr;
    type = MslMessage::MsgNone;
    session = nullptr;
    result = nullptr;
    request.init();
}

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

MslMessagePool::MslMessagePool()
{
    setName("MslMessage");
    setObjectSize(sizeof(MslMessage));
    fluff();
}

MslMessagePool::~MslMessagePool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
MslPooledObject* MslMessagePool::alloc()
{
    return new MslMessage();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
MslMessage* MslMessagePool::newMessage()
{
    return (MslMessage*)checkout();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
