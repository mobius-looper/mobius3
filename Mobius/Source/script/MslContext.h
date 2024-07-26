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

class MslContext
{
  public:
    MslContext() {}
    virtual ~MslContext() {}

    // the id of the context that is talking to the environment
    virtual void MslContextId mslGetContextId() = 0;

    // perform an action that invokes a function or assigns a parameter
    virtual void mslAction(class UIAction* a) = 0;

    // find the value of an external symbol
    virtual bool mslQuery(class Query* q) = 0;

    // initialize a wait state
    virtual bool mslWait(class MslWait* w) = 0;

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
