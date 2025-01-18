/**
 * A collection of ParameterCategoryTree's for each track type.
 */

#pragma once

#include <JuceHeader.h>

#include "ParameterCategoryTree.h"

class SessionTrackTrees : public juce::Component
{
  public:

    SessionTrackTrees();
    ~SessionTrackTrees();

    void load(class Provider* p);

    // eventually will need a type enumeration
    void showMidi(bool midi);

    void resized() override;

  private:

    ParameterCategoryTree audioTree;
    ParameterCategoryTree midiTree;
    bool showingMidi = false;
    
};

