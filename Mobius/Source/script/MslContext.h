/**
 * The interface of an object that provides runtime services to the script
 * environment.  In practice there will only be two of these, one maintained
 * by Supervisor when scripts are being managed outside the audio thread
 * and one by MobiusKernel when scripts are being managed inside the audio thread.
 */

#pragma once

#include <JuceHeader.h>

/**
 * Enumeration of the contexts an MSL session can be running in.
 * Within Shell, this could be divided into the two UI and Maintenance
 * threads but those are already blocking each other with a Juce MessageLock
 * so it is safe to cross reference things between them.  This would change
 * if other non-blocking threads are added.
 */
typedef enum {
    MslContextNone,
    MslContextKernel,
    MslContextShell
} MslContextId;

/**
 * Access to things in the context may encounter errors that are of interest
 * to the script author.  Because errors are arbitrary strings a buffer is provided
 * to deposit the message without dynamic memory allocation.  While these could be passed
 * by value on the stack, they are usually contained within another object such
 * as MslQuery and MslAction.
 */
class MslContextError
{
  public:

    MslExternalError() {strcpy(error, "");}
    ~MslExternalERror() {}

    static const int MslContextErrorMax;
    char error[MslContextErrorMax];

    void setError(const char* msg) {
        strncpy(error, msg, sizeof(error));
    }
};

/**
 * MslQuery is a bi-directional model containg the state necessary to GET something.
 * The MslExternal is the handle to the external variable whose value is to
 * be retrieved.
 *
 * The context obtains the value in an appropriate way and leaves it in the
 * MslValue container that is provided.  Currently this must be a single atomic value
 * as no interface is yet provided for the context to allocate new MslValues to
 * construct a list.  Revisit this when necessary...
 *
 * If an error is detected during the query, a message may be left in the MslContextError
 * message buffer.
 *
 */
public MslQuery
{
  public:

    MslQuery() {}
    ~MslQuery() {}

    MslExternal* external = nullptr;
    
    MslValue value;
    MslError error;
};

/**
 * MslAction defines a collection of state necessary to DO something.
 * Actions are used for two things: calling a function or assigning a variable.
 *
 * The "target" of the action is an MslExternal representing a function or variable.
 * The "value" is the value to assign the variable, or a list of values representing
 * the function arguments.
 *
 * If an error is detected during the query, a message may be left in the message
 * string array.
 *
 * todo: if we get using "keyword arguments" a name will be needed in the MslValue
 * list.
 *
 */
public MslAction
{
  public:

    MslAction() {}
    ~MslAction() {}

    MslExternal* external = nullptr;
    MslValue* arguments = nullptr;

    MslValue result;
    MslError error;

    // opaque pointer to an object in the context representing an asynchronous
    // event that has been scheduled to handle the action
    // this may be used in a Wait
    void* event = nullptr;
    
};

/**
 * A pure virtual interface of an object providing services to the MSL environment.
 */
class MslContext
{
  public:
    MslContext() {}
    virtual ~MslContext() {}

    // the id of the context that is talking to the environment
    virtual MslContextId mslGetContextId() = 0;

    // resolve a name to an MslExternal
    // todo: may want more complex falure messages beyond just true/false
    virtual bool mslResolve(juce::String name, MslExternal* ext);

    // perform a query
    virtual bool mslQuery(MslQuery* query);

    // perform an action
    virtual bool mslAction(MslAction* ation);

    // initialize a wait state
    // for errors, an error buffer is supplied as an argument rather
    // than inside the MslWait, reconsider this
    virtual bool mslWait(class MslWait* w, MslContextError* error) = 0;

    // say something somewhere
    // intended for diagnostic messages from the script
    // could model this with an action but it is used frequently and
    // can have a simpler interface
    virtual void mslEcho(const char* msg) = 0;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
