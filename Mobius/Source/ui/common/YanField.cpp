
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../JuceUtil.h"
#include "ColorPopup.h"
#include "YanField.h"
#include "YanForm.h"

//////////////////////////////////////////////////////////////////////
//
// Spacer
//
//////////////////////////////////////////////////////////////////////

// sign, wish we could sprinkle these without needing an actual object
// but the location in the OwnedArray is awkward

YanSpacer::YanSpacer() : YanField("")
{
}

YanSpacer::~YanSpacer()
{
}

int YanSpacer::getPreferredWidth()
{
    return 0;
}

//////////////////////////////////////////////////////////////////////
//
// Input
//
//////////////////////////////////////////////////////////////////////

YanInput::YanInput(juce::String label, int argCharWidth, bool argReadOnly) : YanField(label)
{
    charWidth = argCharWidth;
    readOnly = argReadOnly;

    text.addListener(this);
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

void YanInput::setListener(Listener* l)
{
    listener = l;
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

void YanInput::setInt(int i)
{
    text.setText(juce::String(i), juce::NotificationType::dontSendNotification);
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

void YanInput::labelTextChanged(juce::Label* l)
{
    (void)l;
    if (listener != nullptr)
      listener->inputChanged(this);
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

//////////////////////////////////////////////////////////////////////
//
// Radio
//
//////////////////////////////////////////////////////////////////////

YanRadio::YanRadio(juce::String label) : YanField(label)
{
}

YanRadio::~YanRadio()
{
}

void YanRadio::setListener(YanRadio::Listener* l)
{
    listener = l;
}

void YanRadio::setButtonLabels(juce::StringArray labels)
{
    // todo: allow reconfiguration
    if (buttons.size() > 0) {
        Trace(1, "YanRadio: Can't reconfigure button labels");
    }
    else if (labels.size() > 0) {

        int index = 0;
        for (auto name : labels) {
            juce::ToggleButton* b = new juce::ToggleButton(name);
            addAndMakeVisible(b);
            buttons.add(b);

            // SimpleRadio comments said "unable to make b->onClick work, see comments at the top"
            // not sure what that meant
            b->addListener(this);

            // assume for now that we want a mutex radio, could be optional
            // comments from SimpleRadio:
            // what is the scope of the radio group id, just within this component
            // or global to the whole application?
            // "To find other buttons with the same ID, this button will search through its sibling components for ToggleButtons, so all the buttons for a particular group must be placed inside the same parent component.
            // so it seems to be local which is good
            b->setRadioGroupId(1);

            // SimpleRadio wasn't smart since it was only used for numbers
            // we could do better
            int guessWidth = 50;
            b->setSize(guessWidth, 20);

            if (initialSelection == index)
              b->setToggleState(true, juce::dontSendNotification);
            index++;
        }
    }
}

void YanRadio::setButtonCount(int count)
{
    if (count > 0) {
        juce::StringArray labels;
        for (int i = 0 ; i < count ; i++) {
            labels.add(juce::String(i + 1));
        }
        setButtonLabels(labels);
    }
}

int YanRadio::getPreferredWidth()
{
    int width = 0;
    for (auto button : buttons)
      width += button->getWidth();
    return width;
}

/**
 * By default button lables are painted on the right.
 * No obvious way to change that to the left.  Chatter suggests
 * overriding the paint method but it's not that hard to manage
 * an array of labels oursleves.  Someday...
 */
void YanRadio::resized()
{
    int buttonOffset = 0;
    for (auto button : buttons) {
        // ugh, arguments are x,y not top,left
        button->setTopLeftPosition(buttonOffset, 0);
        buttonOffset += button->getWidth();
    }
}

void YanRadio::setSelection(int index)
{
    if (buttons.size() == 0) {
        // haven't rendered yet
        initialSelection = index;
    }
    else if (index >= 0 && index < buttons.size()) {
        juce::ToggleButton* b = buttons[index];
        b->setToggleState(true, juce::dontSendNotification);
    }
    else {
        Trace(1, "YanRadio: Index out of range %d", index);
    }
}

int YanRadio::getSelection()
{
    int selection = -1;
    for (int i = 0 ; i < buttons.size() ; i++) {
        juce::ToggleButton* b = buttons[i];
        if (b->getToggleState()) {
            selection = i;
            break;
        }
    }
    return selection;
}

/**
 * We are not really interested in buttonClicked, the thing
 * that created this wrapper component is.  Yet another level
 * of listener, really need to explore lambdas someday.
 *
 * Weird...when dealing with a radio group we get buttonClicked
 * twice, apparently once when turning off the current button
 * and again when turning another one on.  In the first state
 * none of the buttons will have toggle state true, don't call
 * the listener in that case.
 */
void YanRadio::buttonClicked(juce::Button* b)
{
    (void)b;
    // could be smarter about tracking selections without iterating
    // over the button list
    int selection = getSelection();
    // ignore notifications of turning a button off
    if (selection >= 0) {
        if (listener != nullptr)
          listener->radioSelected(this, selection);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Combo
//
//////////////////////////////////////////////////////////////////////

YanCombo::YanCombo(juce::String label) : YanField(label)
{
    combobox.addListener(this);

    // various junk from Field
    
    // figure out how to make this transparent
    combobox.setColour(juce::ComboBox::ColourIds::backgroundColourId, juce::Colours::white);
    combobox.setColour(juce::ComboBox::ColourIds::textColourId, juce::Colours::black);
    combobox.setColour(juce::ComboBox::ColourIds::outlineColourId, juce::Colours::black);
    // "base color for the button", what's this?
    // combobox.setColour(juce::ComboBox::ColourIds::buttonColourId, juce::Colours::black);
    combobox.setColour(juce::ComboBox::ColourIds::arrowColourId, juce::Colours::black);
    combobox.setColour(juce::ComboBox::ColourIds::focusedOutlineColourId, juce::Colours::red);
    
    addAndMakeVisible(combobox);
}

YanCombo::~YanCombo()
{
}

void YanCombo::setListener(YanCombo::Listener* l)
{
    listener = l;
}

void YanCombo::setWidthUnits(int units)
{
    widthUnits = units;
}

void YanCombo::setItems(juce::StringArray names)
{
    // todo: detect if this was already added
    combobox.clear();
    int id = 1;
    int maxChars = 0;
    for (auto name : names) {
        combobox.addItem(name, id);
        id++;

        if (name.length() > maxChars)
          maxChars = name.length();
    }

    // the box also needs to be wide enough to show the pull-down chevron on the right
    // not sure how wide the default is
    int arrowWidth = 24;

    // override the maxChars calculated from the values if the user
    // pass down a desired width
    if (widthUnits > 0)
      maxChars = widthUnits;

    // the usual guessing game
    int charWidth = 12;

    setSize((maxChars * charWidth) + arrowWidth, 12);

    combobox.setSelectedId(1, juce::NotificationType::dontSendNotification);
}

int YanCombo::getPreferredWidth()
{
    // calculated when the items were added
    return getWidth();
}

void YanCombo::setSelection(int index)
{
    combobox.setSelectedId(index + 1);
}

int YanCombo::getSelection()
{
    return combobox.getSelectedId() - 1;
}

juce::String YanCombo::getSelectionText()
{
    return combobox.getText();
}

void YanCombo::resized()
{
    combobox.setBounds(getLocalBounds());
}

void YanCombo::comboBoxChanged(juce::ComboBox* box)
{
    (void)box;
    if (listener != nullptr) {
        int selection = getSelection();
        listener->comboSelected(this, selection);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
