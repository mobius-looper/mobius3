/**
 * A component that displays a configurable set of track
 * status elements and controls for one track.
 *
 * Found at the bottom of MobiusDisplay.
 * Mainted in a set by TrackStrips
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/UIAction.h"
#include "../../Provider.h"

class TrackStrip : public juce::Component,
                   public juce::FileDragAndDropTarget
{
  public:
    
    TrackStrip(class TrackStrips*);
    TrackStrip(class FloatingStripElement*);
    ~TrackStrip();

    class Provider* getProvider();
    class MobiusView* getMobiusView();
    class MobiusViewTrack* getTrackView();

    bool isDocked() {
        return (strips != nullptr);
    }
    
    void setFloatingConfig(int i);
    void setFollowTrack(int i);
    
    int getTrackIndex();
    
    bool isActive() {  
        return (followTrack < 0 || focusedTrack == followTrack);
    }
    
    int getPreferredWidth();
    int getPreferredHeight();
    
    void configure();
    void update(class MobiusView* view);

    void resized() override;
    void paint(juce::Graphics& g) override;
    void mouseDown(const class juce::MouseEvent& event) override;

    void doAction(class UIAction* action);

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    
  private:

    // parent when we're in the docked strips
    class TrackStrips* strips;

    // parent when we're in a floating status element
    class FloatingStripElement* floater;
    
    // taking a different approach than StatusArea and
    // allocating these dynamicall since you don't usually
    // want that many of them
    juce::OwnedArray<class StripElement> elements;

    // the track to follow, -1 means the active track
    int followTrack = -1;

    // the floating configuration to use, 0 is the first
    int floatingConfig = 0;

    // the last focused track, used to draw a white border or
    // to refresh the floating strip
    int focusedTrack = 0;

    // the last state of the audio track activation border
    bool lastActive = false;

    // true if we're an "outer" drop target, meaning any available
    // loop may be dropped into, if the strip contains a LoopStack
    // that overrides the outer target
    bool outerDropTarget = false;
    bool lastDropTarget = false;
    
    // action to send when clicked in the doc to select tracks
    UIAction trackSelectAction;
    
    class StripElement* createStripElement(class StripElementDefinition* def);
    class StripElement* createNewStripElement(class UIElementDefinition* def);

};


    
