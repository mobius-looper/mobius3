/**
 * A component that displays a list of track controls for all tracks.
 *
 * Found at the bottom of MobiusDisplay.
 * 
 * The child components are StripElements and can be configured.
 */

#pragma once

#include <JuceHeader.h>

#include "../../Supervisor.h"

class TrackStrips : public juce::Component
{
  public:
    
    TrackStrips(class MobiusDisplay*);
    ~TrackStrips();

    void configure();
    
    int getPreferredHeight();
    int getPreferredWidth();
    
    void update(class MobiusState* state);

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    class MobiusDisplay* display;
    juce::OwnedArray<class TrackStrip> tracks;
    int dropTarget = -1;
    
    int getDropTarget(int x, int y);
    void setDropTarget(int index);
    
};


    
