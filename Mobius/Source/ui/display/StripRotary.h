/**
 * Extend StripElement to add management or a rotary slider.
 * The StripElementDefinition must be one that is associated with
 * a Parameter.
 *
 * Most of the strip elements are Parameter rotaries so we can
 * reduce almost of all the logic into the base class.   Subclasses
 * just provide ways to get/set the underlying value from MobiusState.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/UIAction.h"

class StripRotary : public StripElement, public juce::Slider::Listener
{
  public:
    
    StripRotary(class TrackStrip* parent, class StripElementDefinition* def);
    ~StripRotary();

    int getPreferredHeight() override;
    int getPreferredWidth() override;

    void update(class MobiusState* state) override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    
    // have to track drag start/end to prevent async update()
    // from slamming the current value back before we can tell
    // it to change the value
    void sliderValueChanged(juce::Slider* slider) override;
    void sliderDragStarted(juce::Slider* slider) override;
    void sliderDragEnded(juce::Slider* slider) override;

    virtual int getCurrentValue(class MobiusTrackState* track) = 0;
    
    // can we do this without messing up the Slider
    // will we even get here?
    void mouseDown(const juce::MouseEvent& e) override;


  protected:

    juce::Slider slider;
    UIAction action;
    
    int value;
    bool dragging;

};

//
// Specific Parameter rotaries
//

class StripOutput : public StripRotary
{
  public:
    StripOutput(class TrackStrip* parent);
    ~StripOutput() {};
    int getCurrentValue(class MobiusTrackState* track);
};
    
class StripInput : public StripRotary
{
  public:
    StripInput(class TrackStrip* parent);
    ~StripInput() {};
    int getCurrentValue(class MobiusTrackState* track);
};
    
class StripFeedback : public StripRotary
{
  public:
    StripFeedback(class TrackStrip* parent);
    ~StripFeedback() {};
    int getCurrentValue(class MobiusTrackState* track);
};
    
class StripAltFeedback : public StripRotary
{
  public:
    StripAltFeedback(class TrackStrip* parent);
    ~StripAltFeedback() {};
    int getCurrentValue(class MobiusTrackState* track);
};
    
class StripPan : public StripRotary
{
  public:
    StripPan(class TrackStrip* parent);
    ~StripPan() {};
    int getCurrentValue(class MobiusTrackState* track);
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

