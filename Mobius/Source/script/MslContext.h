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

class MslContext
{
  public:
    MslContext() {}
    virtual ~MslContext() {}

    // locate the root of the Mobius installation tree
    // for Environment, this can be where we locate the standard script library files
    // maybe make this more explicit, like getScriptLibraryLocation
    virtual juce::File mslGetRoot() = 0;
    
    // locate a read-only copy of the MobiusConfig
    virtual class MobiusConfig* mslGetMobiusConfig() = 0;

    // perform an action that invokes a function or assigns a parameter
    virtual void mslDoAction(class UIAction* a) = 0;

    // find the value of an external symbol
    virtual bool mslDoQuery(class Query* q) = 0;

    //
    // From here down are more random services that need thought
    // about what needs to have better abstraction
    //

};


