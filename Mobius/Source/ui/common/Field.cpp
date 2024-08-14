/**
 * An object model for form fields that are rendered as Juce components.
 * 
 * DESIGN NOTES
 *
 * Field could either inherit directly from Component or it could be
 * a parallel model that generates Components.  Tradeoffs are unclear
 * at this time, start with them being Components.  The introduction of managed/unmanaged
 * moves toward making these not Components but simply adding things to the parent.
 * 
 * Rendering
 *
 * Fields have deferred rendering as Juce components.  You create a Field then
 * set the various display properties then call render() to construct the necessary
 * Juce components that implement it.  Rendering will calculate and set the initial
 * minimum size.  This size is normally left alone.  
 *
 * Juce label attachments vs. managed labels
 *
 * Juce has some basic mechanisms for attaching a label to a component
 * and following it around.  

 * When you attach a label to a component it is displayed to the left or top of the component.
 * You need to position the attached component so that the label had enough room
 * to display on the left or above, just attaching it does not create some sort of wrapper
 * component that understands this.  It follows the component around but is it's own
 * component that just happens to get bounds automatically from the attached component,
 * I guess filling whatever space is available.
 * Examples show giving it Justification::right but that doesn't seem to matter.
 *
 * Forum comments from 2021:
 *  It looks like juce::Label::attachToComponent is used in a few places in the codebase by other JUCE
 * components, so it’s likely it was only added to suit the needs of those other components.
 *
 * This may be enough, but I'm kind of liking having Field manage the positioning
 * directly rather than using attachments.  This fits better with the notion of "unmanaged"
 * labels which let's the container own the label.
 *
 * Managed vs. Unmanaged labels
 *
 * A managed label is when the label component is a child of the Field and the field
 * is responsible for positining it.  The label still sets it's own size.  The bounds for the field
 * must be large enough to accomodate the label.  To support label positioning and justification
 * the parent must give the field information about where the label is to be displayed and how
 * to position it.
 *
 * An unmanaged label is when the label component is a child of the parent and the field
 * does not position it.  The parent handles all label positioning.
 *
 * Managed is the default.
 * 
 * Sizing notes:
 *
 * Since rendered compoennts are all lightweight unless we're using native look and feel
 * they don't seem to have any specified preferred size, they'll adapt to the size we give them.
 * We'll guess some reasonable values.
 *
 * Parent size overrides:
 *
 * Fields will start out with their preferred minimum size.  If the parent gives it a larger
 * size, we could try to center within that area (assuming right justified labels). This won't
 * happen yet.
 *
 * Component required sizing
 *
 * It seems to be counter to the Juce way of thinking to let things size themselves
 * they always want th parent to size it and make it big enough for the unknown amount
 * 
 * From the forums...
 * Minimum widths:
 *   TextButton : getStringWidth(button.text) + button.height[/]
 *   ToggleButton : getStringWidth(button.text) + min(24, button.height) + 8[/]
 *   TextEditor : getStringWidth(text.largestWordcontent) + leftIndent (default 4px) + 1 + borderSize.left (default 1px) + borderSize.right (default 1px) => default sum is 7px [/]
 *    ComboBox : same as TextEditor[/][/list]
 *
 * Checkboxes (toggle buttons) are weird.
 * There is always some padding on the left but none on the right.
 * There seems to be no way to control this other than jiggering the x position provided you're
 * within a component with enough space.  If you just want to have a Checkbox by itself
 * not wrapped in anything you get the pad unless maybe a negative x position would work.
 * Probably have to do a custom button with it's own paint.  Someday...
 */

#include <string>
#include <sstream>

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../JuceUtil.h"
#include "HelpArea.h"
#include "Field.h"

//////////////////////////////////////////////////////////////////////
//
// Field
//
//////////////////////////////////////////////////////////////////////

/**
 * Fields have a name property which conflicts with Component.
 * Could use getFieldName instead.
 */
Field::Field(juce::String argName, juce::String argDisplayName, Field::Type argType)
{
    juce::Component::setName("Field");  // class sname for debugging
    name = argName;
    displayName = argDisplayName;
    type = argType;
    initLabel();
}

Field::Field(const char* argName, Field::Type argType)
{
    juce::Component::setName("Field");  // class sname for debugging
    name = juce::String(argName);
    type = argType;
    initLabel();
}

Field::~Field()
{
}

void Field::setAllowedValues(const char** arg)
{
    allowedValues = juce::StringArray(arg);
}

void Field::setAllowedValues(juce::StringArray& src)
{
    allowedValues = src;
}

void Field::setAllowedValueLabels(const char** arg)
{
    allowedValueLabels = juce::StringArray(arg);
}

void Field::setAllowedValueLabels(juce::StringArray& src)
{
    allowedValueLabels = src;
}

void Field::initLabel()
{
    // Set up the label with reasonable defaults that can
    // be overridden after construction but before rendering

    // TODO: use JLabel and stop using attachments
    label.setText (getDisplayableName(), juce::dontSendNotification);
    label.setFont (juce::Font (16.0f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, juce::Colours::orange);
    // the default is centeredLeft, I think this is used when the
    // label is given bounds larger than necessary to contain the font text
    label.setJustificationType (juce::Justification::left);
}

/**
 * Kludge for checkboxes in the binding panels.
 */
void Field::addAnnotation(int width)
{
    addAndMakeVisible(annotation);
    // if this were ever more general will need more control
    // over the background vs. text color
    annotation.setColour (juce::Label::textColourId, juce::Colours::white);
    annotation.setSize(width, 20);
}

/**
 * Kludge for the checkbox used in binding panels.
 * Allow checkboxes to have an annotation which is a read-only
 * label to the right of them where we can show things of interest.
 */
void Field::setAnnotation(juce::String text)
{
    // have to send notifications in order for it to repaint when set
    annotation.setText(text, juce::NotificationType::sendNotificationAsync);
}

//////////////////////////////////////////////////////////////////////
//
// Rendering
//
//////////////////////////////////////////////////////////////////////

/**
 * This is normally done when first rendered, but for fields that are combo
 * boxes of structure names, they need to be have their allowed values refreshed
 * every time they are displayed.
 */
void Field::updateAllowedValues(juce::StringArray& src)
{
    allowedValues = src;
    if (renderType == RenderType::Combo) {
        combobox.clear();
        int maxChars = 0;
        for (int i = 0 ; i < allowedValues.size() ; i++) {
            juce::String s = allowedValues[i];
            if (s.length() > maxChars)
              maxChars = s.length();
            // note that item ids must be non-zero
            combobox.addItem(s, i + 1);
        }

        // todo: in theory maxChars could be larger now and needs to make the field bigger
        // forms aren't that responsive yet and it is ordinarilly long enough
    }
}

/**
 * Once all properties of the field are specified, render
 * it with appropriate juce components and calculate initial
 * minimum display size.
 */
void Field::render()
{
    renderLabel();

    // render methods will set renderType and renderer
    switch (type) {
        case Type::Integer: { renderInt(); } break;
        case Type::String: { renderString(); } break;
        case Type::Boolean: { renderBool(); } break;
    }

    if (renderer != nullptr) {
        addAndMakeVisible(renderer);
        // make attachment optional
        if (!unmanagedLabel)
          label.attachToComponent(renderer, true);
    }

    // note well: label.attachToComponent seems to clear out the width
    // I guess because it tries to size based on the current size
    // of the component which is too small at this point
    // so we need to render and set it's size after attaching.
    // still don't fully understand how attachment works, but
    // we're likely to stop using that anyway
    renderLabel();

    // set the initial value if we have one
    if (!value.isVoid())
      loadValue();

    // calculate bounds using both the label and the renderer
    juce::Rectangle<int> size = getMinimumSize();

    setSize(size.getWidth(), size.getHeight());
}

/**
 * Label properties have been set by now.
 * Here calculate minimum size and add it as a child if necessary.
 */
void Field::renderLabel()
{
    juce::Font font = label.getFont();
    int width = font.getStringWidth(label.getText());
    int height = (int)font.getHeight();

    // this seems to be not big enough, Juce did "..."
    width += 10;
    label.setSize(width, height);

    // label is a child comonent unless the parent wants to manage it
    if (!unmanagedLabel) {
        addAndMakeVisible(label);
        label.addMouseListener(this, true);
    }
}

/**
 * Render a string field as either a text field, a combo box, or a select list
 * The size of the internal components will be set.
 * TODO: If we allow size overrides after construction, this might
 * need to carry forward to these, particularly the select list which we could
 * make taller.  Hmm, some forms might want a bigger select list to auto-size.
 */
void Field::renderString()
{
    // most sizing will be derived from the label font
    juce::Font font = label.getFont();
    int charHeight = (int)font.getHeight();
    // for width it is unclear, need to dig into Juce source
    // I think it is a multiple of kerning, but unclear how to get that
    int charWidth = font.getStringWidth(juce::String("M"));

    if (allowedValues.isEmpty()) {
        renderType = RenderType::Text;
        renderer = &textbox;
        
        textbox.addListener(this);
        textbox.setEditable(true);
        textbox.setColour (juce::Label::backgroundColourId, juce::Colours::darkgrey);
        // from the demo
        // textbox.onTextChange = [this] { uppercaseText.setText (inputText.getText().toUpperCase(), juce::dontSendNotification); };

        // let's start with font size plus a little padding
        int chars = widthUnits;
        // if not specified pick something big enough for a typical name
        if (chars == 0) 
          chars = 20;

        // add a little padding on top and bottom
        textbox.setSize(chars * charWidth, charHeight + 4);
    }
    else if (!multi) {
        renderType = RenderType::Combo;
        renderer = &combobox;
        
        int maxChars = 0;
        for (int i = 0 ; i < allowedValues.size() ; i++) {
            juce::String s = allowedValues[i];
            if (s.length() > maxChars)
              maxChars = s.length();
            // note that item ids must be non-zero
            combobox.addItem(s, i + 1);
        }

        // when we set this programatically don't send notifications
        // since our field listeners generally expect to be notified
        // only when the user does it manually
        combobox.setSelectedId(1, juce::NotificationType::dontSendNotification);

        // figure out how to make this transparent
        combobox.setColour(juce::ComboBox::ColourIds::backgroundColourId, juce::Colours::white);
        combobox.setColour(juce::ComboBox::ColourIds::textColourId, juce::Colours::black);
        combobox.setColour(juce::ComboBox::ColourIds::outlineColourId, juce::Colours::black);
        // "base color for the button", what's this?
        // combobox.setColour(juce::ComboBox::ColourIds::buttonColourId, juce::Colours::black);
        combobox.setColour(juce::ComboBox::ColourIds::arrowColourId, juce::Colours::black);
        combobox.setColour(juce::ComboBox::ColourIds::focusedOutlineColourId, juce::Colours::red);

        // the box also needs to be wide enough to show the pull-down chevron on the right
        // not sure how wide the default is
        int arrowWidth = 24;

        // override the maxChars calculated from the values if the user
        // pass down a desired width
        if (widthUnits > 0)
          maxChars = widthUnits;
        
        combobox.setSize((maxChars * charWidth) + arrowWidth, charHeight + 4);
        combobox.addListener(this);
    }
    else {
        renderType = RenderType::List;
        renderer = &listbox;
        listbox.setValues(allowedValues);
        listbox.setValueLabels(allowedValueLabels);

        juce::StringArray& displayValues = allowedValues;
        if (allowedValueLabels.size() > 0)
          displayValues = allowedValueLabels;

        int maxChars = 0;
        for (int i = 0 ; i < displayValues.size() ; i++) {
            juce::String s = displayValues[i];
            if (s.length() > maxChars)
              maxChars = s.length();
        }

        // if they bothered to specify a width use that
        if (widthUnits > maxChars)
          maxChars = widthUnits;

        // the number of lines to display before scrolling starts
        // may be explicitly specified
        int rows = heightUnits;
        if (rows == 0) {
            rows = allowedValues.size();
            // constrain this for long lists, could make the max configurable
            if (rows > 4)
              rows = 4;
        }
        
        listbox.setSize(maxChars * charWidth, (charHeight * rows) + 4);
    }
}

void Field::renderInt()
{
    // most sizing will be derived from the label font
    juce::Font font = label.getFont();
    int charHeight = (int)font.getHeight();
    // for width it is unclear, need to dig into Juce source
    // I think it is a multiple of kerning, but unclear how to get that
    int charWidth = font.getStringWidth(juce::String("M"));

    if (min == 0 && max < 100) {
        renderType = RenderType::Text;
        renderer = &textbox;
        
        textbox.addListener(this);
        textbox.setEditable(true);
        // make them look different for testing
        // this controls the background of the text area, default text
        // color seems to be white
        textbox.setColour (juce::Label::backgroundColourId, juce::Colours::black);

        int chars = widthUnits;
        // if not specified pick something big enough for a typical name
        // like combo boxes, something is wrong with the math and this is
        // much larger than needed
        if (chars == 0) 
          chars = 8;

        // add a little padding on top and bottom
        textbox.setSize(chars * charWidth, charHeight + 4);
    }
    else {
        // allow this to be preset
        if (renderType != RenderType::Rotary)
          renderType = RenderType::Slider;
        renderer = &slider;
        slider.setRange((double)min, (double)max, 1);

        // For non-rotary sliders, this is the text for the number
        // that appears in the box to the left of the slider
        slider.setColour(juce::Slider::ColourIds::textBoxTextColourId, juce::Colours::white);
        
        if (renderType == RenderType::Rotary) {
            slider.setSliderStyle(juce::Slider::SliderStyle::Rotary);
        }

        if (renderType == RenderType::Rotary) {
            // make these big enough to be usable
            // these are weird and need more thought
            // like regular sliders they have a value box on the left, then the rotary
            // they need quite a bit of height to make them usable
            // what is interesting is that the compoents seem to be centering themselves
            // within the allowed height, not sure why
            int boxWidth = (widthUnits > 0) ? widthUnits : 40;
            int totalWidth = (charWidth * 8) + boxWidth;
            slider.setSize(totalWidth, boxWidth);
        }
        else {
            // sliders could adjust their width based on the range of values?
            // there are two parts to this, the box on the left that displays the value
            // and the slider, default box width seems to be about 8-10 characters
            // make the slider 100 for now
            // should be basing height on font height
            // hmm, having a hard time controlling the box width, it seems to take
            // a fixed proportion of the width given
            int width =  (charWidth * 6) + 100;
            slider.setSize(width, 20);
        }
    }
}

void Field::renderBool()
{
    renderType = RenderType::Check;
    renderer = &checkbox;
    
    addAndMakeVisible(checkbox);
    // could use this to put the text within the button area and not use labels
    //checkbox.setButtonText("foo");

    // Color selection is going to be entirely dependent on the background
    // color of the container, which we don't know, black text looks fine on
    // a light background, but not black
    // Mobius leans toward a dark scheme, so assume that, but would be nice
    // if some coloring hints could be passed down

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
}

/**
 * Calculate the minimum bounds for this field after rendering.
 *
 * Don't really need to be using Rectangle here since
 * we only care about width and height, but using Point
 * for x,y felt weird.  
 */
juce::Rectangle<int> Field::getMinimumSize()
{
    juce::Rectangle<int> size;
    int totalWidth = 0;
    int maxHeight = 0;

    // start with the attached label
    if (!unmanagedLabel) {
        totalWidth = label.getWidth();
        maxHeight = label.getHeight();
    }
    
    // assume renderer has left a suitable size
    if (renderer != nullptr) {
        totalWidth += renderer->getWidth();
        int rheight = renderer->getHeight();
        if (rheight > maxHeight)
          maxHeight = rheight;
    }

    // kludge for checkbox annotations
    totalWidth += annotation.getWidth();

    size.setWidth(totalWidth);
    size.setHeight(maxHeight);

    return size;
}

int Field::getLabelWidth()
{
    return label.getWidth();
}

int Field::getRenderWidth()
{
    return renderer->getWidth();
}

/**
 * Listener when we're configured as a ComboBox
 * Pass it along to our listener
 */
void Field::comboBoxChanged(juce::ComboBox* box)
{
    (void)box;
    // note that just "listener" is inherited from Component so
    // had to qualify the member name, or could have used a prefix
    if (fieldListener != nullptr)
      fieldListener->fieldChanged(this);
}

/**
 * Listener when we're configured as a text box.
 */
void Field::labelTextChanged(juce::Label* l)
{
    (void)l;
    if (fieldListener != nullptr)
      fieldListener->fieldChanged(this);
}

//////////////////////////////////////////////////////////////////////
//
// Layout
//
//////////////////////////////////////////////////////////////////////

/**
 * Layout the subcomponents.
 * If we have a managed label, adjust the position of the renderer
 * relative to the label.  Width and Height have already been set
 * for both subcomponents.
 */
void Field::resized()
{
    // do them all the same way for now, may want more control
    if (renderer != nullptr) {
        int offset = 0;

        if (!unmanagedLabel) {
            // TODO: need more justification options besides
            // left adjacent
            offset += label.getWidth();
        }
        
        renderer->setTopLeftPosition(offset, 0);
        annotation.setTopLeftPosition(offset + renderer->getWidth() + 4, 0);
    }
}

void Field::paint(juce::Graphics& g)
{
    (void)g;
    // temporarily border it for testing
    //g.setColour(juce::Colours::red);
    //g.drawRect(getLocalBounds(), 1);
}

//////////////////////////////////////////////////////////////////////
//
// Value Management
//
//////////////////////////////////////////////////////////////////////


/**
 * Set the value of a field and propagate it to the components.
 * If the field has not been rendered yet, we just save it to the
 * var member until later.  This makes it a little easier to
 * build fields without needing to understand the order dependency.
 */
void Field::setValue(juce::var argValue)
{
    // hack, if this is a combobox, allow the value to be
    // an integer item index
    if (renderer == &combobox && argValue.isInt()) {
        int index = (int)argValue;
        if (index >= 0 && index < allowedValues.size())
          value = allowedValues[index];
        else {
            // out of range, ignore
        }
    }
    else {
        value = argValue;
    }

    loadValue();
}

/**
 * Copy the intermediate value into the component after rendering.
 */
void Field::loadValue()
{
    // what will the components do with a void value?

    if (renderer == &textbox) {
        // See this discussion on why operator String() doesn't work
        // if you add (juce::String) cast, just pass it without a cast, subtle
        // https://forum.juce.com/t/cant-get-var-operator-string-to-compile/36627
        textbox.setText(value, juce::dontSendNotification);
    }
    else if (renderer == &checkbox) {
        checkbox.setToggleState((bool)value, juce::dontSendNotification);
    }
    else if (renderer == &combobox) {
        // should only get here if we had allowedValues
        int itemId = 0;
        for (int i = 0 ; i < allowedValues.size() ; i++) {
            juce::String s = allowedValues[i];
            // can't do (s == value) or (s == (juce::String)value)
            if (s == value.toString()) {
                itemId = i + 1;
            }
        }
        
        combobox.setSelectedId(itemId, juce::NotificationType::dontSendNotification);
    }
    else if (renderer == &slider) {
        slider.setValue((double)value, juce::NotificationType::dontSendNotification);
    }
    else if (renderer == &listbox) {
        // compare values value to allowedValues and
        // set all the included values
        juce::String csv = value.toString();
        // not supporting display names yet
        // don't know if Juce has any CSV utilities
        juce::StringArray values;
        JuceUtil::CsvToArray(csv, values);
        listbox.setSelectedValues(values);
    }
    else {
        // Field hasn't been rendered yet
    }
}

/**
 * Return the current field value.
 * If the field has been rendered get it from the component.
 */
juce::var Field::getValue()
{
    if (renderer == &textbox) {
        if (type == Type::Integer) {
            juce::String s = textbox.getText();
            value = s.getIntValue();
        }
        else {
            value = textbox.getText();
        }
    }
    else if (renderer == &checkbox) {
        value = checkbox.getToggleState();
    }
    else if (renderer == &combobox) {
        // needs heavy redesign since ParameterField now
        // exclusively deals with enum ordinals
        int selected = combobox.getSelectedId();
        // I suppose this could have the concept of nothing
        // selected, we don't support that, just auto select
        // the first one
        if (selected > 0)
          selected--;
        value = allowedValues[selected];
    }
    else if (renderer == &slider) {
        value = (int)slider.getValue();
    }
    else if (renderer == &listbox) {
        juce::StringArray selected;
        listbox.getSelectedValues(selected);
        juce::String csv = JuceUtil::ArrayToCsv(selected);
        value = csv;
    }

    return value;
}

/**
 * If this is a text field coerce to an integer.
 * If tis is a combobox, return the selected item index.
 * For listBox, could return index if this isn't a multi-select
 * but I don't have a use for that at the moment.
 */
int Field::getIntValue()
{
    int ival = 0;
    
    if (renderer == &combobox) {
        // returns zero if no item selected
        int selected = combobox.getSelectedId();
        // this should match getValue above which will
        // auto select the first one if nothing selected
        if (selected > 0)
          ival = selected - 1;
    }
    else {
        // this may coerce for text fields of type Integer
        ival = (int)getValue();
    }

    return ival;
}

juce::String Field::getStringValue()
{
    return getValue().toString();
}

const char* Field::getCharValue()
{
    return getValue().toString().toUTF8();
}

bool Field::getBoolValue() {
    return (bool)getValue();
}

//////////////////////////////////////////////////////////////////////
//
// Mouse/Help
//
//////////////////////////////////////////////////////////////////////

void Field::mouseEnter(const juce::MouseEvent& e)
{
    (void)e;
    // Trace(2, "Field: mouseEnter %s\n", name.toUTF8());
    if (helpArea != nullptr)
      helpArea->showHelp(name);
}

void Field::mouseExit(const juce::MouseEvent& e)
{
    (void)e;
    // Trace(2, "Field: mouseExit %s\n", name.toUTF8());
    // clear it or latch it?
    // this works as long as a mouseExit is always sent BEFORE
    // the mouseEnter for the next component
    if (helpArea != nullptr) {
        helpArea->clear();
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
