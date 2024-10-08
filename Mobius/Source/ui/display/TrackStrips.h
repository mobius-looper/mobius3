/**
 * A component that displays a list of track controls for all tracks.
 *
 * Found at the bottom of MobiusDisplay.
 * 
 * The child components are StripElements and can be configured.
 */

#pragma once

#include <JuceHeader.h>

class TrackStrips : public juce::Component
{
  public:
    
    TrackStrips(class MobiusDisplay*);
    ~TrackStrips();

    class Provider* getProvider();
    class MobiusView* getMobiusView();

    void configure();
    
    int getPreferredHeight();
    int getPreferredWidth();
    
    void update(class MobiusView* view);

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    class MobiusDisplay* display = nullptr;
    juce::OwnedArray<class TrackStrip> tracks;
    bool dualTracks = false;
    int dropTarget = -1;
    
    int getDropTarget(int x, int y);
    void setDropTarget(int index);
    
};


    
