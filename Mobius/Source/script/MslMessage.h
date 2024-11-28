/**
 * Object used to pass reequests from one context to another for
 * session management.
 *
 * This is specific to sending sustain/repeat notifications but could
 * be generalized for other uses.  One could be script cancel.
 */

#pragma once

#include "MslObjectPool.h"
#include "MslConstants.h"

class MslMessage : public MslPooledObject
{
  public:

    typedef enum {
        MsgNone,
        MsgTransition,
        MsgNotification,
        MsgCompletion,
        MsgResult
    } Type;
    
    MslMessage();
    ~MslMessage();
    void poolInit() override;

    // message list chain pointer
    MslMessage* next = nullptr;

    // what it is
    Type type = MsgNone;

    // for Transition and Completion, the session we're transitioning
    class MslSession* session = nullptr;

    // for the Notification type, the specific notification
    MslNotificationFunction notification = MslNotificationNone;

    // for trigger notifications the option request arguments
    // todo: could just pass the entire Request here which
    // makes sense and would take the place of some of the Notification
    // types but that would need to be pooled too
    class MslBinding* bindings = nullptr;
    class MslValue* arguments = nullptr;

    // for some of the Notifications, an associated trigger
    int triggerId = 0;

    // todo: for Result, need the sessionId or some other way to
    // identify it.  Work through what goes in the result, certainly
    // error and informational messages, name/value pairs so probably
    // reuse MslBinding for data and MslValue for messages, but messages
    // also need a level and other context

};    

class MslMessagePool : public MslObjectPool
{
  public:

    MslSequencePool();
    virtual ~MslSequencePool();

    class MslMessage* newMessage();

  protected:
    
    virtual MslPooledObject* alloc() override;
    
};
