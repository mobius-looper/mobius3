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
#include "MslRequest.h"

class MslMessage : public MslPooledObject
{
  public:

    typedef enum {
        MsgNone,
        MsgTransition,
        MsgRequest,
        MsgResult
    } Type;
    
    MslMessage();
    ~MslMessage();
    void poolInit() override;

    // message list chain pointer
    MslMessage* next = nullptr;

    // what it is
    Type type = MsgNone;

    // for MsgTransition, the session we're transitioning
    class MslSession* session = nullptr;

    // for MsgRequest, a copy of the request provided
    // by the application, a member object so we don't have to mess with
    // pooling for this since it is rarely cloned
    MslRequest request;

    // for MsgResult, the result object the shell is to take
    // ownership of
    class MslResult* result = nullptr;

    // todo: for Result, need the sessionId or some other way to
    // identify it.  Work through what goes in the result, certainly
    // error and informational messages, name/value pairs so probably
    // reuse MslBinding for data and MslValue for messages, but messages
    // also need a level and other context

};    

class MslMessagePool : public MslObjectPool
{
  public:

    MslMessagePool();
    virtual ~MslMessagePool();

    class MslMessage* newMessage();

  protected:
    
    virtual MslPooledObject* alloc() override;
    
};
