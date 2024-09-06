/**
 * Object holding details about the state of a compilation unit.
 * This is the model accessible to the application.  It is transient
 * and detached from the models used by the environment at runtime.
 *
 * This can be returned by several environment calls and not all fields
 * will be relevant for every call.  Usually they are related to a specific
 * compilation unit (file, scriptlet), but in some cases may just contain
 * error or warning messages.
 *
 * It's really more of a generic "call result" object, but MslResult is
 * used for the session model, and this is more general information about
 * what is installed in the environment, not execution control.
 */

#pragma once

#include <JuceHeader.h>

#include "MslError.h"
#include "MslLinkage.h"
#include "MslCollision.h"

class MslDetails
{
  public:

    MslDetails() {}
    ~MslDetails() {}

    // every call may return errors and some return warnings
    juce::OwnedArray<class MslError> errors;
    juce::OwnedArray<class MslError> warnings;

    //
    // Unit Information
    // When the call is realted to the status of a compilation unit
    // these fields will be filled in
    //

    // unique id of the unit
    juce::String id;

    // the name of the unit if it has a callable body
    juce::String name;

    // true if the unit contents have been "published" and available for use
    // a unit can be installed but not published, publishing is denied
    // if there are name collisions that have not been resolved
    bool published = false;

    // the linkages that have been published for this unit
    // these represent the functions and variables exported by the unit
    juce::Array<class MslLinkage*> linkages;
    
    // current name collisions that prevent it from being published
    juce::OwnedArray<class MslCollision> collisions;

    // currrent unresolved symbols
    // a unit may install with nothing unresolved, but unloading another unit
    // may cause references in other units to become unresolved
    juce::StringArray unresolved;

    /**
     * Package a random installation error up in the MslError wrapper
     */
    void addError(juce::String msg) {
        MslError* err = new MslError();
        err->setDetails(msg.toUTF8());
        errors.add(err);
    }

};

    
