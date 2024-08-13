/**
 * A basic single line text input component with a label,
 * auto-sizing, and some layout options.
 *
 * When dealing with labels and input text boxes, I REALLY dont like
 * the top-down resizing philosophy.  I have a text box that should be 20
 * characters wide, tall enough to be nicely visible, and it has an arbitrary
 * label in front of it that I want to display without sqaushing it too much.
 * The preferred width is a combination of those things, not some arbitrary bounds
 * passed down from the container, which would have to duplicate this layout
 * logic everywhere you want to stick a simple input field.  Maybe I just
 * don't "get it" yet, but it seems a whole hell of a lot easier just to make
 * a component that figures out a good size for itself, and let the parent
 * work around that.  Especially in initial exploratory mode where I'm adding
 * and removing components a lot and don't have time to think about a grand layout
 * strategy for every container that wants to have a damn text box.
 *
 * Added the option for the input label to be read-only so this can be also be used
 * to display labeled information that can't be changed.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "BasicInput.h"

const int BasicInputDefaultHeight = 20;
//const int BasicInputDefaultChars = 10;
const int BasicInputLabelGap = 4;

BasicInput::BasicInput(juce::String argLabel)
{
    charWidth = 0;
    
    label.setText(argLabel, juce::NotificationType::dontSendNotification);
    // do we need to set a font?
    // assume we're dark on light
    label.setColour(juce::Label::textColourId, juce::Colours::black);
    label.setJustificationType(juce::Justification::left);

    addAndMakeVisible(label);
}

BasicInput::BasicInput(juce::String argLabel, int numChars, bool argReadOnly)
{
    charWidth = numChars;
    readOnly = argReadOnly;
    
    label.setText(argLabel, juce::NotificationType::dontSendNotification);
    // do we need to set a font?
    // assume we're dark on light
    label.setColour(juce::Label::textColourId, juce::Colours::black);
    label.setJustificationType(juce::Justification::left);
    
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
    
    addAndMakeVisible(label);
    addAndMakeVisible(text);

    autoSize();
}

void BasicInput::setLabelCharWidth(int numChars)
{
    labelCharWidth = numChars;
//    if (labelCharWidth > 0)
//      autoSize();
}

void BasicInput::setLabelColor(juce::Colour c)
{
    label.setColour(juce::Label::textColourId, c);
}

void BasicInput::setLabelRightJustify(bool b)
{
    if (b)
      label.setJustificationType(juce::Justification::centredRight);
    else
      label.setJustificationType(juce::Justification::centredLeft);
}

/**
 * Calculate a reasonable size based on the label and desired number of
 * characters in the text field.
 */
void BasicInput::autoSize()
{
    // let the label breathe
    juce::Font font (BasicInputDefaultHeight);

    // you typically want something wide enough for the thing
    // being typed in, numbers are a few characters and names are more
    // you think "I'd like this 20 letters wide" not "I'd like this 429 pixels wide"

    // todo: I have various calculatesion that use "M" width but this always results
    // in something way too large when using proportional fonts with mostly lower case
    // here let's try using e instead
    int emWidth = font.getStringWidth("e");
    int textWidth = emWidth * charWidth;
    int labelWidth = 0;
    if (labelCharWidth)
      labelWidth = emWidth * labelCharWidth;
    else
      labelWidth = font.getStringWidth(label.getText());
    
    // todo: remember the proportion of the label within the total default width
    // so this can be resized later and keep the same approximate balance between
    // the label and the text box?

    int totalWidth = labelWidth + textWidth + BasicInputLabelGap;

    setSize(totalWidth, BasicInputDefaultHeight);
}

BasicInput::~BasicInput()
{
}

void BasicInput::addListener(juce::Label::Listener* l)
{
    if (readOnly)
      Trace(1, "BasicInput: Adding a listener to a read-only component, is that what you wanted?\n");
    text.addListener(l);
}

/**
 * Well, after all that work, the parent said something else.  Parents.
 * Need to divide the space between the label and the text box.
 * Favor the text box, and hope the label fits.
 */
void BasicInput::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    juce::Font font ((float)getHeight());
    // M is too large, experiment with e
    int emWidth = font.getStringWidth("e");
    int textWidth = emWidth * charWidth;

    int labelWidth = 0;
    if (labelCharWidth)
      labelWidth = emWidth * labelCharWidth;
    else
      labelWidth = font.getStringWidth(label.getText());

    label.setBounds(area.removeFromLeft(labelWidth));
    text.setBounds(area.removeFromLeft(textWidth));
}

void BasicInput::paint(juce::Graphics& g)
{
    (void)g;
}

juce::String BasicInput::getText()
{
    return text.getText();
}

int BasicInput::getInt()
{
    return text.getText().getIntValue();
}

void BasicInput::setText(juce::String s)
{
    text.setText(s, juce::NotificationType::dontSendNotification);
}

void BasicInput::setAndNotify(juce::String s)
{
    text.setText(s, juce::NotificationType::sendNotification);
}

//////////////////////////////////////////////////////////////////////
//
// Checkbox
//
//////////////////////////////////////////////////////////////////////

BasicCheckbox::BasicCheckbox(juce::String label) : BasicInput(label)
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
    checkbox.setSize(21, 20);

    addAndMakeVisible(checkbox);

    autoSize();
}

BasicCheckbox::~BasicCheckbox()
{
}

/**
 * Calculate a reasonable size based on the label and width of the checkbox.
 * !! Needs refactoring to share Label handling
 */
void BasicCheckbox::autoSize()
{
    // let the label breathe
    juce::Font font (BasicInputDefaultHeight);

    int emWidth = font.getStringWidth("e");
    int labelWidth = 0;
    if (labelCharWidth)
      labelWidth = emWidth * labelCharWidth;
    else
      labelWidth = font.getStringWidth(label.getText());
    
    int totalWidth = labelWidth + BasicInputLabelGap + checkbox.getWidth();

    setSize(totalWidth, BasicInputDefaultHeight);
}

void BasicCheckbox::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    juce::Font font ((float)getHeight());
    // M is too large, experiment with e
    int emWidth = font.getStringWidth("e");
    int labelWidth = 0;
    if (labelCharWidth)
      labelWidth = emWidth * labelCharWidth;
    else
      labelWidth = font.getStringWidth(label.getText());

    label.setBounds(area.removeFromLeft(labelWidth));
    checkbox.setBounds(area.removeFromLeft(checkbox.getWidth()));
}

//////////////////////////////////////////////////////////////////////
//
// ColorChooser
//
//////////////////////////////////////////////////////////////////////

BasicColorChooser::BasicColorChooser(juce::String label) : BasicInput(label)
{
    
    text.setColour(juce::Label::textColourId, juce::Colours::white);
    text.setColour(juce::Label::backgroundColourId, juce::Colours::black);

    text.setText("Choose...", juce::NotificationType::dontSendNotification);

    addAndMakeVisible(text);

    text.addMouseListener(this, true);
    
    charWidth = 20;
    autoSize();
}

void BasicColorChooser::mouseDown(const juce::MouseEvent& e)
{
    (void)e;
    Trace(2, "BasicColorSelector::mouseDown");
}
