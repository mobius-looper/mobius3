/**
 * The interface of an object that provides runtime services to the script
 * environment.  In practice there will only be two of these, one maintained
 * by Supervisor when scripts are being managed outside the audio thread
 * and one by MobiusKernel when scripts are being managed inside the audio thread.
 *
 * Ideally model dependencies like MobiusConfig and UIAction would be abstracted
 * away as well, so the Environment only has to deal with classes that it defines.
 */

#pragma once

#include <JuceHeader.h>

/**
 * Enumeration of the contexts an MSL session can be running in.
 */
typedef enum {
    MslContextNone,
    MslContextKernel,
    MslContextShell
} MslContextId;

/**
 * Container used to convey back to the MSL engine an opaque pointer representing
 * an event that will be used to schedule a call or an assignment in the future.
 * This handle can be used in an MslWait to wait for that event to finish.
 */
class MslContextEvent
{
  public:
    MslContextEvent() {}
    ~MslContextEvent() {}

    void* event = nullptr;
};

/**
 * Container used to convey back to the MSL engine an error message that resulted
 * from a call or an assignment.
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

class MslContext
{
  public:
    MslContext() {}
    virtual ~MslContext() {}

    // the id of the context that is talking to the environment
    virtual MslContextId mslGetContextId() = 0;

    // resolve a name to an MslExternal
    virtual bool mslResolve(juce::String name, MslExternal* ext);

    // call an external function
    // the external function is identified by a previously resolved
    // MslExternal, arguments to the function are passed as a list of MslValues
    // the return value is conveyed by setting a value in the MslValue used for returns
    // false is returned to indiciate error, in which case the return MslValue may have
    // an error message string
    virtual bool mslCall(MslExternal* ext, MslValue* arguments, MslValue* result,
                         MslContextEvent* event, MslContextError* error);

    // assign a value to a variable
    virtual bool mslAssign(MslExternal* ext, MslValue* value,
                           MslContextEvent* event, MslContextError* error);

    // retrieve the value of a variable
    // returning false means there was an error
    virtual bool mslQuery(MslExternal* ext, MslValue* value, MslContextError* error);

    // initialize a wait state
    virtual bool mslWait(class MslWait* w, MslContextError* error) = 0;


    // obsolete, will delete when mslCall etc are working
    // perform an action that invokes a function or assigns a parameter
    virtual void mslAction(class UIAction* a) = 0;

    // obsolete, will delete when mslQuery(external) etc are working
    // find the value of an external symbol
    virtual bool mslQuery(class Query* q) = 0;

    // say something somewhere
    // should we do levels here to so we can get rid of the
    // Message and Alert callbacks in MobiusListener?
    virtual void mslEcho(const char* msg) = 0;

    //
    // try to get rid of these, file handling is now done by ScriptClerk
    //
    
    // locate the root of the Mobius installation tree
    // for Environment, this can be where we locate the standard script library files
    // maybe make this more explicit, like getScriptLibraryLocation
    virtual juce::File mslGetRoot() = 0;
    
    // locate a read-only copy of the MobiusConfig
    virtual class MobiusConfig* mslGetMobiusConfig() = 0;

    //
    // From here down are more random services that need thought
    // about what needs to have better abstraction
    //

};
