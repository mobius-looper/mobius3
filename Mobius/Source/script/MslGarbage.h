/**
 * Holder for objects that may be referenced by compilations and
 * still in use by running sessions.
 *
 * Currently this only has MslCompilation units since you can't
 * unload function and variable definitiosn independently of the unit.
 * This may change.
 */

#pragma once

#include <JuceHeader.h>

class MslGarbage
{
  public:

    MslGarbage();
    ~MslGarbage();

    void add(class MslCompilation* unit) {
        units.add(f);
    }

  protected:

    juce::OwnedArray<MslCompilation> units;

};


    
