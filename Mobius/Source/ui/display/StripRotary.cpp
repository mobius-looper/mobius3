/**
 * Extend StripElement to add management or a rotary slider.
 * The StripElementDefinition must be one that is associated with
 * a Parameter.
 *
 * I don't see an immediate need to have these without a Paameter
 * But could add more state to support that.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Provider.h"
#include "../../model/Symbol.h"
#include "../../ui/MobiusView.h"

#include "../JuceUtil.h"
#include "Colors.h"
#include "TrackStrip.h"
#include "StripElement.h"
#include "StripRotary.h"

// not using value boxes any more
//const int BoxWidth = 30;
//const int BoxHeight = 20;

const int RotaryDiameter = 60;
const int LabelFontHeight = 14;

// kludge: the rotary draws with a large gap at the bottom
// can't find a way to reduce that so we'll draw the label
// over the bottom part to tighten it up
// so the LabelGap will actually be negative
const int LabelGap = -10;

StripRotary::StripRotary(class TrackStrip* parent, StripElementDefinition* def) :
    StripElement(parent, def)
{
    // these always correspond to Parameters with a 0-127 range
    // fuck, now that UIParameter is gone, we don't have a great place to get
    // these during construction, just make it 0 to 127 which is all we ever used
    // and figure out a better way to handle this if necessary
    //slider.setRange((float)def->parameter->low, (float)def->parameter->high);
    slider.setRange((float)0, (float)127);


    // some of these are now defaults in CustomRotary
    slider.setColour(juce::Slider::ColourIds::textBoxTextColourId, juce::Colours::black);
    slider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
    // position, readOnly, width, height
    // disable the text box, it adds clutter and we don't need to
    // type in numbers for things like output level
    // started with TextBoxBelow but it never did display the number
    // which although the old interface did, we can do without since
    // you can't do that in the middle of the component without overriding
    // the look and feel
    slider.setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    slider.addListener(this);

    addAndMakeVisible(slider);

    // now that we have Symbol, don't need to be passing the UIParameter around
    // in the StripElementDefinition
    action.symbol = strip->getProvider()->getSymbols()->intern(definition->getName());

    // see if the slider can pass mouse events up
    // second arg is wantsEventsForAllNestedChildComponents
    // yes, this does work, but the slider still responds to both left and right
    // buttons, we can't intercept it for popup menus or dragging without
    // it interfering with the slider
    slider.addMouseListener(this, false);

    dragging = false;
}


StripRotary::~StripRotary()
{
}

void StripRotary::mouseDown(const juce::MouseEvent& e)
{
    (void)e;
    //Trace(2, "StripRotary: mouseDown\n");
}

/**
 * We always want these labeled.
 * Have some flexibility on the font here..
 * Not showing the slider value box so don't have to deal with it
 */
int StripRotary::getPreferredWidth()
{
    int maxWidth = 0;
    
    // Parameters should always have display names
    const char* label = definition->getDisplayableName();
    if (label != nullptr) {
        juce::Font font(JuceUtil::getFont(LabelFontHeight));
        maxWidth = font.getStringWidth(label);
    }

    if (RotaryDiameter > maxWidth)
      maxWidth = RotaryDiameter;

    return maxWidth;
}

int StripRotary::getPreferredHeight()
{
    return RotaryDiameter + LabelFontHeight + LabelGap;
}

void StripRotary::resized()
{
    // since the label is usually wider than the
    // minimum slider size, the rotary will also
    // enlarge to fill the space, not bad but
    // depending on how they stack may want to constrain it

    // sigh, still way too much default space around the rotary
    // how can we constrain it, it doesn't seem to track the width
    //slider.setBounds(0, 0, RotaryDiameter, RotaryDiameter);
    // let it fill width to center
    slider.setBounds(0, 0, getWidth(), RotaryDiameter);
}

void StripRotary::paint(juce::Graphics& g)
{
    // trying to figure out where the knob bottom is
    // because there is a large gap
    //g.setColour(juce::Colours::red);
    //g.drawRect(0, 0, getWidth(), RotaryDiameter);
    
    const char* label = definition->getDisplayableName();
    int top = RotaryDiameter + LabelGap;
    g.setColour(juce::Colour(MobiusBlue));
    g.setFont(JuceUtil::getFont(LabelFontHeight));
    g.drawText(juce::String(label), 0, top, getWidth(), LabelFontHeight,
               juce::Justification::centred);
}

void StripRotary::sliderDragStarted(juce::Slider*)
{
    dragging = true;
}

void StripRotary::sliderDragEnded(juce::Slider*)
{
    dragging = false;
}

/**
 * Ask the subclass to pull the current value from MobiusViewTrack
 * compare it to our current value and if different, update
 * the slider and repaint.
 */
void StripRotary::update(MobiusView* view)
{
    (void)view;
    if (!dragging) {
        MobiusViewTrack* track = getTrackView();

        // subclass implements this
        int current = getCurrentValue(track);

        if (current != value) {
            value = current;
            // since we are tracking values, do not send
            // notifications which would cause sliderValueChanged
            // and can mess up test scripts trying to set this parameter
            slider.setValue((double)value, juce::NotificationType::dontSendNotification);
            slider.repaint();
        }
    }
}

/**
 * After the slider changes, refresh out tracking value and
 * perform an action on that parameter.
 */
void StripRotary::sliderValueChanged(juce::Slider* obj)
{
    // capture the value in local state so we don't 
    // trigger a repaint on the next update
    value = (int)obj->getValue();
    
    action.value = value;

    // this will add track scope and pass to Provider
    strip->doAction(&action);
}

//////////////////////////////////////////////////////////////////////
//
// Parameter Rotaries
//
// Only need these because Parameter doesn't know how to get things
// out of a MobiusViewTrack.
//
// TODO: This sucks as usual as it means that it means that only
// a few things can be rotaries.  We should be able to use these for
// any parameter and use MobiusInterface::getParameter to get them
// and a UIAction to set them.  More like how DAWs behave.
//
//////////////////////////////////////////////////////////////////////

// Output

StripOutput::StripOutput(class TrackStrip* parent) :
    StripRotary(parent, StripDefinitionOutput)
{
}

int StripOutput::getCurrentValue(MobiusViewTrack* track)
{
    return track->outputLevel;
}
    
// Input

StripInput::StripInput(class TrackStrip* parent) :
    StripRotary(parent, StripDefinitionInput)
{
}

int StripInput::getCurrentValue(MobiusViewTrack* track)
{
    return track->inputLevel;
}
    
// Feedback

StripFeedback::StripFeedback(class TrackStrip* parent) :
    StripRotary(parent, StripDefinitionFeedback)
{
}

int StripFeedback::getCurrentValue(MobiusViewTrack* track)
{
    return track->feedback;
}

// AltFeedback

StripAltFeedback::StripAltFeedback(class TrackStrip* parent) :
    StripRotary(parent, StripDefinitionAltFeedback)
{
}

int StripAltFeedback::getCurrentValue(MobiusViewTrack* track)
{
    return track->altFeedback;
}
    
// Pan

StripPan::StripPan(class TrackStrip* parent) :
    StripRotary(parent, StripDefinitionPan)
{
}

int StripPan::getCurrentValue(MobiusViewTrack* track)
{
    return track->pan;
}
    
/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
