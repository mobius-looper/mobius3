/**
 * Object used to pass reequests from one context to another for
 * session management.
 *
 * This is specific to sending sustain/repeat notifications but could
 * be generalized for other uses.  One could be script cancel.
 */

#pragma once

#include "MslConstants.h"

class MslMessage
{
  public:
    MslMessage();
    ~MslMessage();
    void init();
    
    // message list chain pointer
    MslMessage* next = nullptr;

    int triggerId = 0;
    MslNotificationFunction notification = MslNotificationNone;

    class MslBinding* bindings = nullptr;
    class MslValue* arguments = nullptr;

};    
