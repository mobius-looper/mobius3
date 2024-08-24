
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../JuceUtil.h"
#include "ColorPopup.h"
#include "YanField.h"
#include "YanForm.h"

//////////////////////////////////////////////////////////////////////
//
// Input
//
//////////////////////////////////////////////////////////////////////

YanInput::YanInput(juce::String label, int argCharWidth, bool argReadOnly) : YanField(label)
{
    charWidth = argCharWidth;
    readOnly = argReadOnly;

    text.setColour(juce::Label::textColourId, juce::Colours::white);
    text.setColour(juce::Label::backgroundColourId, juce::Colours::black);
    
    if (!readOnly) {
        text.setEditable(true);

        // clicking on the textbox after it has a value seems to always put
        // the cursor at the front, and I almost always want it at the end
        // for some reason this uses lambdas rather than listeners to detect changes
        text.onEditorShow = [this](){
            juce::TextEditor* ed = text.getCurrentTextEditor();
            if (ed != nullptr)
              ed->moveCaretToEnd();
        };
    }

    addAndMakeVisible(text);
}

void YanInput::setValue(juce::String value)
{
    text.setText(value, juce::NotificationType::dontSendNotification);
}

void YanInput::setAndNotify(juce::String value)
{
    text.setText(value, juce::NotificationType::sendNotification);
}

juce::String YanInput::getValue()
{
    return text.getText();
}

int YanInput::getInt()
{
    return text.getText().getIntValue();
}

int YanInput::getPreferredWidth()
{
    int preferred = 0;
    int chars = (charWidth > 0) ? charWidth : 20;

    // let the form win for now
    int availableHeight = YanForm::RowHeight;

    // todo: I have various calculatesion that use "M" width but this always results
    // in something way too large when using proportional fonts with mostly lower case
    // here let's try using e instead
    juce::Font font(JuceUtil::getFont(availableHeight));
    int emWidth = font.getStringWidth("e");
    preferred = emWidth * chars;

    return preferred;
}

void YanInput::resized()
{
    text.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// Checkbox
//
//////////////////////////////////////////////////////////////////////

/**
 * Force a size on these so they look usable.
 * See commentary in the constructor for how these values were determined.
 */
const int YanCheckboxWidth = 21;

/**
 * Since we let the YanForm determine the row height, this isn't used.
 */
const int YanCheckboxHeight = 20;

YanCheckbox::YanCheckbox(juce::String label) : YanField(label)
{
    // comments scraped from Field.cpp
     
    // for my checkboxes, textColourId doesn't seem to do anything, maybe because
    // I'm managing labels a different way
    checkbox.setColour(juce::ToggleButton::ColourIds::textColourId, juce::Colours::white);
    // this is the color of the checkmark
    checkbox.setColour(juce::ToggleButton::ColourIds::tickColourId, juce::Colours::red);
    // this is the color of the rounded rectangle surrounding the checkbox
    checkbox.setColour(juce::ToggleButton::ColourIds::tickDisabledColourId, juce::Colours::white);

    // make it big enough to be useful, might want to adapt to font size?
    // seems to have a little padding on the left, is this what
    // the setConnected edge flags do?
    // ugh, button sizing is weird
    // 10,10 is too small and the bounds it needs seems to be more than that and it
    // gets clipped on the right
    // these don't do anything, sounds like they're only hints for LookAndFeel
    /*
    int edgeFlags =
        juce::Button::ConnectedEdgeFlags::ConnectedOnLeft |
        juce::Button::ConnectedEdgeFlags::ConnectedOnRight |
        juce::Button::ConnectedEdgeFlags::ConnectedOnTop |
        juce::Button::ConnectedEdgeFlags::ConnectedOnBottom;
    */
    checkbox.setConnectedEdges(0);

    // buttons are weird
    // without a label and just a 20x20 component there are a few pixels of padding
    // on the left and the right edge of the button overlaps with the test bounding rectangle
    // we draw around the field that's fine on the right, but why the left?
    // don't see a way to control that

    // The clipping on the right seems to be caused by a checkbox having a required width
    // 20x20 results in one pixel being shaved off the right edge 21x20 has a normal
    // border. Seems to be a dimension requirement relative to the height.  +1 works
    // here but may change if you make it taller
    checkbox.setSize(YanCheckboxWidth, YanCheckboxHeight);

    addAndMakeVisible(checkbox);
}

int YanCheckbox::getPreferredWidth()
{
    return YanCheckboxWidth;
}

void YanCheckbox::setValue(bool b)
{
    checkbox.setToggleState(b, juce::dontSendNotification);
}

bool YanCheckbox::getValue()
{
    return checkbox.getToggleState();
}

void YanCheckbox::resized()
{
    checkbox.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// ColorChooser
//
//////////////////////////////////////////////////////////////////////

const int YanColorChooserWidth = 100;

YanColorChooser::YanColorChooser(juce::String label) : YanField(label)
{
    text.setColour(juce::Label::textColourId, juce::Colours::white);
    text.setColour(juce::Label::backgroundColourId, juce::Colours::black);

    text.setText("Choose...", juce::NotificationType::dontSendNotification);
    text.addMouseListener(this, true);

    // let color start white to match the text
    color = juce::Colours::white.getARGB();
    
    addAndMakeVisible(text);
}

int YanColorChooser::getPreferredWidth()
{
    return YanColorChooserWidth;
}

void YanColorChooser::resized()
{
    text.setBounds(getLocalBounds());
}

void YanColorChooser::setValue(int argb)
{
    color = argb;
    text.setColour(juce::Label::textColourId, juce::Colour(argb));
}

int YanColorChooser::getValue()
{
    return color;
}

void YanColorChooser::mouseDown(const juce::MouseEvent& e)
{
    (void)e;
    //Trace(2, "YanColorSelector::mouseDown");

    // kludge: this needs a container big enough to show the popup
    // the form we're within isn't usually big enough
    // we're not really supposed to know what the parent hierarchy looks like
    // but it's usually two levels up

    // this will be the YanForm
    juce::Component* container = getParentComponent();
    // this will be the GroupEditor
    container = container->getParentComponent();

    popup.show(container, this, color);
}

void YanColorChooser::colorSelected(int argb)
{
    //Trace(2, "YanColorSelector::Color selected %d", argb);
    color = argb;
    if (listener != nullptr)
      listener->colorSelected(color);

    // until we show a color box change the text color
    text.setColour(juce::Label::textColourId, juce::Colour(argb));
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
