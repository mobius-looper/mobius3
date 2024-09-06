/**
 * Object representing the state of an installed compilation unit.
 * 
 * This will be returned by the call to MslEnvironment::install
 * and also by MslEnvironment::getInstallStatus.
 * 
 */

#pragma once

#include <JuceHeader.h>

class MslInstallation
{
  public:

    MslInstallation() {}
    ~MslInstallation() {}

    // unique id of the unit
    // for a new installation of an anonymous unit (scriptlet) this
    // be an internally generated identifier that the application must
    // now use when referring to things in the unit
    juce::String id;

    // true if the unit contents have been "published" and available for use
    // a unit can be installed but not published, publishing is denied
    // if there are name collisions that have not been resolved
    bool published = false;

    // the linkages that have been published for this unit
    // these represent the functions and variables exported by the unit
    juce::Array<MslLinkage*> linkages;

    // errors detected during installation
    // installations don't normally have errors, but who knows, the night is young
    juce::OwnedArray<class MslError> errors;
    juce::OwnedArray<class MslError> warnings;
    
    // current name collisions that prevent it from being published
    juce::OwnedArray<MslCollision> collisions;

    // currrent unresolved symbols
    // a unit may install with nothing unresolved, but unloading another unit
    // may cause references in other units to become unresolved
    juce::StringArray unresolved;

};

    
