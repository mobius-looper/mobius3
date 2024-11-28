
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
    delete bindings;
    // so does this
    delete arguments;

    // session I'm not sure about ownership
    if (session != nullptr)
      Trace(1, "MslMessage: Leaking session");

    // the message itself itself does not cascade
}

void MslMessage::poolInit()
{
    next = nullptr;
    type = MslMessage::MsgNone;
    session = nullptr;
    notification = MslNotificationNone;
    bindings = nullptr;
    arguments = nullptr;
    triggerId = 0;
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
