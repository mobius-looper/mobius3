/**
 * A collection of SessionEditorTrees's for each track type.
 */

#pragma once

#include <JuceHeader.h>

#include "SessionEditorTree.h"

class SessionTrackTrees : public juce::Component
{
  public:

    SessionTrackTrees();
    ~SessionTrackTrees();

    void initialize(class Provider* p);

    // eventually will need a type enumeration
    void showMidi(bool midi);

    void resized() override;

  private:

    SessionEditorTree audioTree;
    SessionEditorTree midiTree;
    bool showingMidi = false;
    
};

