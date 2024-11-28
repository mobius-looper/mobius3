
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
}

void MslMessage::init()
{
    next = nullptr;
    triggerId = 0;
    notification = MslNotificationNone;
    bindings = nullptr;
    arguments = nullptr;
}
