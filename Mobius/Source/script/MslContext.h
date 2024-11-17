/**
 * The interface of an object that provides runtime services to the script
 * environment.  In practice there will only be two of these, one maintained
 * by Supervisor when scripts are being managed outside the audio thread
 * and one by MobiusKernel when scripts are being managed inside the audio thread.
 */

#pragma once

#include <JuceHeader.h>

#include "MslValue.h"

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

    MslContextError() {strcpy(error, "");}
    ~MslContextError() {}

    static const int MslContextErrorMax = 128;
    char error[MslContextErrorMax];

    void setError(const char* msg) {
        strncpy(error, msg, sizeof(error));
    }

    bool hasError() {
        return (strlen(error) > 0);
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
class MslQuery
{
  public:

    MslQuery() {}
    ~MslQuery() {}

    class MslExternal* external = nullptr;

    // actions may have a scope identifier when using "IN"
    // so external references need that too
    // currently this is a track number but should be more flexible
    // about abstract scope names
    int scope = 0;
    
    MslValue value;
    MslContextError error;
};

/**
 * The interface of an object placed in an MslAction that may be used
 * to obtain additional information from the script runtime environment.
 * This is actually an MslSession, but hides the dangerolus parts and reduces
 * the compile time dependencies on the caller.
 */
class MslSessionInterface
{
  public:
    
    virtual ~MslSessionInterface() {}

    /**
     * Obtain the value of a bound variable.
     */
    virtual MslValue* getVariable(const char* name) = 0;
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
class MslAction
{
  public:

    MslAction() {}
    ~MslAction() {}

    // action target, usually a symbol or a library fucntion
    class MslExternal* external = nullptr;

    // positional arguments on a list
    MslValue* arguments = nullptr;

    // script session that created this action, can be by library functions
    // to pull additional information from the script runtime environment
    class MslSessionInterface* session = nullptr;

    // actions may have a scope identifier when using "IN"
    // currently this is a track number but should be more flexible
    // about abstract scope names
    int scope = 0;

    // the value you are supposed to fill in if the function returns a value
    MslValue result;

    // an error message to be returned to the interpreter
    MslContextError error;

    // opaque pointer to an object in the context representing an asynchronous
    // event that has been scheduled to handle the action
    // this may be used in a Wait
    // this will be saved in the MslAsyncState object in the MslSession
    void* event = nullptr;
    int eventFrame = 0;

    // todo: need a model for temporary external variable bindings
    // e.g. parameter overrides
    
};

/**
 * A pure virtual interface of an object providing services to the MSL environment.
 */
class MslContext
{
  public:
    MslContext() {}
    virtual ~MslContext() {}

    //virtual class SymbolTable* getSymbols() = 0;

    // the id of the context that is talking to the environment
    virtual MslContextId mslGetContextId() = 0;

    // resolve a name to an MslExternal
    // todo: may want more complex falure messages beyond just true/false
    virtual bool mslResolve(juce::String name, class MslExternal* ext) = 0;

    // perform a query
    virtual bool mslQuery(MslQuery* query) = 0;

    // perform an action
    virtual bool mslAction(MslAction* action) = 0;

    // initialize a wait state
    // for errors, an error buffer is supplied as an argument rather
    // than inside the MslWait, reconsider this
    virtual bool mslWait(class MslWait* w, MslContextError* error) = 0;

    // say something somewhere
    // intended for diagnostic messages from the script
    // could model this with an action but it is used frequently and
    // can have a simpler interface
    virtual void mslPrint(const char* msg) = 0;

    // let the context know about the installation of a new access point
    // this is where the linkage happens between script objects and the
    // Mobius Symbol table, two ways to do this, let the environment tell
    // the container or have the container ask for everything
    // unclear what works best, since everything comes in through ScriptClerk
    // it is also in a position to install symbols after loading files
    virtual void mslExport(class MslLinkage* link) = 0;

    // get the number of scopes allowed for the "in" statement
    // eventually will need more creative about naming them
    // this may not be necessary if we let mslExpandScope do the work 
    virtual int mslGetMaxScope() = 0;

    // given the name of a symbol within the "in" statement
    // return true if this is a valid scope reference keyword
    virtual bool mslIsScopeKeyword(const char* name) = 0;
    
    // given the name of an abstract scope used with the "in" statement
    // fill in an array of concrete scope numbers for that scope
    virtual bool mslExpandScopeKeyword(const char* name, juce::Array<int>& numbers) = 0;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
