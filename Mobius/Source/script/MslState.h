/**
 * An object containing persistent state the Environment needs to save and restore
 * across instantiations.  The container will ask for this on shutdown and is expected
 * to save it to a file.  On startup, the container reads that file and gives the
 * state back to the environment during the initialization sequence.
 *
 * The initial purpose is to provide storage for "persistent" variables, may have other
 * uses.  The container is free to model the storage format in whatever way is appropriate.
 * XML is not required.
 */

#pragma once

#include <JuceHeader.h>

#include "MslValue.h"

class MslState {

  public:

    class Variable {
      public:
        int scopeId;
        juce::String name;
        MslValue value;
    };

    class Unit {
      public:

        juce::String id;
        juce::OwnedArray<Variable> variables;
    };

    juce::OwnedArray<Unit> units;
};


    
