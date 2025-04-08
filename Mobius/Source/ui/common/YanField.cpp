
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../Services.h"
#include "../JuceUtil.h"

#include "ColorPopup.h"
#include "YanField.h"
#include "YanForm.h"

//////////////////////////////////////////////////////////////////////
//
// FieldLabel
//
//////////////////////////////////////////////////////////////////////

YanFieldLabel::YanFieldLabel(YanField* p)
{
    setName("YanFieldLabel");
    parent = p;
}

/**
 * Drag is only necessary for ParameterForm and only in a few usages.
 * Is it bad to assume we can always be a drag initiator?
 *
 * Since the DropTarget only gets a juce::String describing the thing to drop
 * and where it came from, the convention I'm following is to previx the string
 * with a source identifier followed by an object identifier of some kind.
 * In current use for ParameterForms, the label text is the display name of the Symbol
 */
void YanFieldLabel::mouseDown(const juce::MouseEvent& e)
{
    (void)e;
    if (parent != nullptr) {
        // this is only draggable if it was given a description,
        // in current usage, that is always a Symbol name
        juce::String desc = parent->getDragDescription();
        if (desc.length() > 0) {
            // must be inside something that supports DnD
            juce::DragAndDropContainer* cont = juce::DragAndDropContainer::findParentDragContainerFor(this);
            if (cont != nullptr) {

                // the actual description we pass combines a source identifier
                // with the component description, use a generic YanField prefix
                // until we need something more complex
                juce::String qualifiedDesc = juce::String(DragPrefix) + desc;
                cont->startDragging(qualifiedDesc, this);
            }
        }
    }
}

void YanFieldLabel::setListener(Listener* l)
{
    listener = l;
}

void YanFieldLabel::mouseUp(const juce::MouseEvent& e)
{
    (void)e;
    if (listener != nullptr && parent != nullptr) {

        // !! if dragable need to suppress if we're dragging?
        listener->yanFieldClicked(parent, e);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Field
//
//////////////////////////////////////////////////////////////////////

YanField::YanField()
{
}

YanField::YanField(juce::String argLabel)
{
    label.setText(argLabel, juce::NotificationType::dontSendNotification);
    // make them look like the old Form/Fields
    // notes say bold can make it look too thick in smaller forms
    // may want to dial this back from the justified label in the left column
    // if this adjacent
    //label.setFont (juce::Font (16.0f, juce::Font::bold));
    label.setFont (JuceUtil::getFontf(16.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, juce::Colours::orange);
}

YanField::~YanField()
{
}

void YanField::setOrdinal(int i)
{
    ordinal = i;
}

int YanField::getOrdinal()
{
    return ordinal;
}

void YanField::setLabelListener(YanFieldLabel::Listener* l)
{
    label.setListener(l);
}

void YanField::setTooltip(juce::String tt)
{
    label.setTooltip(tt);
}

void YanField::setLabel(juce::String s)
{
    label.setText(s, juce::NotificationType::dontSendNotification);
}

juce::Label* YanField::getLabel()
{
    return &label;
}

void YanField::setAdjacent(bool b)
{
    bool last = adjacent;
    adjacent = b;
    if (b != last) {
        if (b)
          addAndMakeVisible(&label);
        else
          removeChildComponent(&label);
    }
}

bool YanField::isAdjacent()
{
    return adjacent;
}

int YanField::getPreferredWidth(int rowHeight)
{
    int pwidth = getPreferredComponentWidth();

    if (adjacent) {
        juce::Font font (JuceUtil::getFont(rowHeight));
        int lwidth = font.getStringWidth(label.getText());
        pwidth += lwidth;
        pwidth += 4;
    }
    return pwidth;
}

juce::Rectangle<int> YanField::resizeLabel()
{
    juce::Rectangle<int> area = getLocalBounds();
    if (adjacent) {
        juce::Font font (JuceUtil::getFont(getHeight()));
        int lwidth = font.getStringWidth(label.getText());
        label.setBounds(area.removeFromLeft(lwidth));
        (void)area.removeFromLeft(4);
    }
    return area;
}

void YanField::setDragDescription(juce::String s)
{
    dragDescription = s;
}

juce::String YanField::getDragDescription()
{
    return dragDescription;
}

/**
 * Labels may have three color options: normal, disabled, and explciit.
 * Most fields have a normal default color.  If a field is disabled
 * it will automatically be given an alternate color, typically
 * grey or something dark.
 *
 * In a few (one) case, a label may be given an explicit color to indiciate
 * a special quality of the field.  When a label nas an explicit color
 * the automatic coloration for enabled/disabled does not apply.
 */
void YanField::setLabelColor(juce::Colour c)
{
    explicitLabelColor = c;

    // is this a good way to indiciate "unset"?  What if they want it black?
    if (explicitLabelColor == juce::Colour())
      setNormalLabelColor();
    else
      label.setColour(juce::Label::textColourId, explicitLabelColor);
}

void YanField::unsetLabelColor()
{
    setLabelColor(juce::Colour());
}

void YanField::setNormalLabelColor()
{
    if (disabled)
      label.setColour (juce::Label::textColourId, juce::Colours::grey);
    else
      label.setColour (juce::Label::textColourId, juce::Colours::orange);
}

/**
 * This is normally overridden by the subclass to take the
 * appropriate action.  But it needs to call back here to handle
 * the disable color.
 */
void YanField::setDisabled(bool b)
{
    if (b != disabled) {
        disabled = b;
        if (explicitLabelColor == juce::Colour())
          setNormalLabelColor();
    }
}

bool YanField::isDisabled()
{
    return disabled;
}

//////////////////////////////////////////////////////////////////////
//
// Spacer
//
//////////////////////////////////////////////////////////////////////

YanSpacer::YanSpacer()
{
}

YanSpacer::~YanSpacer()
{
}

int YanSpacer::getPreferredComponentWidth()
{
    return 0;
}

YanSection::YanSection(juce::String label) : YanField(label)
{
}

YanSection::~YanSection()
{
}

int YanSection::getPreferredComponentWidth()
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
            if (ed != nullptr) {
                ed->moveCaretToEnd();
                ed->addListener(this);
            }
            if (listener != nullptr)
              listener->yanInputEditorShown(this);
        };

        text.onEditorHide = [this](){
            if (listener != nullptr)
              listener->yanInputEditorHidden(this);
        };
    }

    addAndMakeVisible(text);
}

void YanInput::setNoBorder(bool b)
{
    noBorder = b;
}

void YanInput::setBackgroundColor(juce::Colour c)
{
    text.setColour(juce::Label::ColourIds::backgroundColourId, c);
}

void YanInput::setDisabled(bool b)
{
    text.setEnabled(!b);
    // keep this in sync so we don't have to overload isDisabled
    YanField::setDisabled(b);
}

void YanInput::textEditorTextChanged(juce::TextEditor& ed)
{
    if (listener != nullptr)
      listener->yanInputEditorChanged(this, ed.getText());
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

int YanInput::getPreferredComponentWidth()
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
    juce::Rectangle<int> textBounds = resizeLabel();
    if (!noBorder)
      textBounds = textBounds.reduced(1);
    
    text.setBounds(textBounds);
}

void YanInput::paint(juce::Graphics& g)
{
    if (!noBorder) {
        g.setColour(juce::Colours::darkgrey);
        g.drawRect(getLocalBounds(), 1);
    }
}

void YanInput::labelTextChanged(juce::Label* l)
{
    (void)l;
    if (listener != nullptr)
      listener->yanInputChanged(this);
}

//////////////////////////////////////////////////////////////////////
//
// YanFile
//
//////////////////////////////////////////////////////////////////////

YanFile::YanFile(juce::String label) : input(label)
{
    button.setButtonText("Choose");
    button.addListener(this);
    
    addAndMakeVisible(input);
    addAndMakeVisible(button);
}

YanFile::~YanFile()
{
    // unusual, but if you kill the parent component containing this field
    // before the async file chooser request is finished, the handler needs to
    // be removed since this object will no longer be valid

    // !! This caused a puzzling destruction ordering problem
    // if you leave the file chooser open and then close the application,
    // this YanFile is not destructed until after ~Supervisor is in the process
    // of being called, calling Prompter here raises an exception, I guess because
    // it has already been destroyed by this time
    // it would be better if Supervisor destroyed everything in the UI first before
    // allowing the automatic destruction process on itself to happen, or maybe
    // make a pass over all the current UI components and tell them to remove themselves
    // from any handler registration they might have
    //if (fileChooser != nullptr)
    //fileChooser->fileChooserCancel(purpose);
}

void YanFile::initialize(juce::String purp, FileChooserService* svc)
{
    purpose = purp;
    fileChooser = svc;
}

int YanFile::getPreferredComponentWidth()
{
    return input.getPreferredComponentWidth() + 50;
}

void YanFile::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    button.setBounds(area.removeFromRight(50));
    input.setBounds(area);
}

void YanFile::buttonClicked(juce::Button* b)
{
    (void)b;
    if (fileChooser == nullptr)
      Trace(1, "YanFile: FileChooserService not available");
    else
      fileChooser->fileChooserRequestFolder(purpose, this);
}

void YanFile::fileChooserResponse(juce::File f)
{
    input.setValue(f.getFullPathName());
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

void YanCheckbox::setDisabled(bool b)
{
    checkbox.setEnabled(!b);
    // keep this in sync so we don't have to overload isDisabled
    YanField::setDisabled(b);
}

int YanCheckbox::getPreferredComponentWidth()
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
    checkbox.setBounds(resizeLabel());
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

int YanColorChooser::getPreferredComponentWidth()
{
    return YanColorChooserWidth;
}

void YanColorChooser::resized()
{
    text.setBounds(resizeLabel());
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
      listener->yanColorSelected(color);

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
            b->setSize(guessWidth, YanForm::RowHeight);

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

int YanRadio::getPreferredComponentWidth()
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
    // how the hell does this work?
    //juce::Rectangle<int> remainder = resizeLabel();
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
          listener->yanRadioSelected(this, selection);
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

void YanCombo::setDisabled(bool b)
{
    combobox.setEnabled(!b);
    // keep this in sync so we don't have to overload isDisabled
    YanField::setDisabled(b);
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

    setSize((maxChars * charWidth) + arrowWidth, YanForm::RowHeight);

    setItemNoNotify(1);
}

/**
 * Internal item selector that makes damn sure notifications are not sent.
 * This can cause infinite loops if the Listener on this causes it's value to
 * be changed again.
 */
void YanCombo::setItemNoNotify(int id)
{
    combobox.setSelectedId(id, juce::NotificationType::dontSendNotification);
}    

// having trouble getting the setItems size to stick
int YanCombo::calculatePreferredWidth()
{
    // something wrong with your math here, for simple combos containing integers less
    // than 10, nothing is displayed except the arrow
    // have to bump it up to 2 digits to get enough space to see single digit numbers
    int maxChars = 2;
    for (int i = 0 ; i < combobox.getNumItems() ; i++) {
        juce::String name = combobox.getItemText(i);
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

    return ((maxChars * charWidth) + arrowWidth);
}

int YanCombo::getPreferredComponentWidth()
{
    // calculated when the items were added
    //return getWidth();
    return calculatePreferredWidth();
}

void YanCombo::setSelection(int index)
{
    setItemNoNotify(index + 1);
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
    combobox.setBounds(resizeLabel());
}

void YanCombo::comboBoxChanged(juce::ComboBox* box)
{
    (void)box;
    if (listener != nullptr) {
        int selection = getSelection();
        listener->yanComboSelected(this, selection);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
