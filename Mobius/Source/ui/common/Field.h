/**
 * An object model for form fields that are rendered as Juce components.
 *
 * A Field is the most basic unit of input in a form. It manages the display
 * of a single value A field may be rendered using a variety of Juce components.
 * Fields will provide a preferred size which may be overridden by the container.
 *
 * You must call the render() method after setting properties of the field
 * to build out the suitable child components.
 *
 * Fields may be of type integer, boolean, string, or enumeration.
 * Enumeration fields will have a list of allowed values which may or not be
 * split into internal and display values.
 *
 * Fields can render an appropriate Juce component automaticaly.
 * Fields may accept an alternative renderStyle to select from among different options.
 *
 * Fields may be multi-valued.
 * 
 * Fields are given a value and will return it after editing.  They do not know
 * where the value came from.
 *
 * Fields may have a listener to respond to changes to the field.
 *
 * A FieldGrid organizes multiple fields into one or more columns.
 * All fields in a grid are displayed.
 *
 * A Form contains one or more grids.
 * If there are multiple grids they are selcted using tabs or
 * some other set selection mechanism.  If a form contains more than one
 * grid, the grid must be named to provide the name for the tab.
 *
 */

#pragma once

#include <JuceHeader.h>
#include "SimpleListBox.h"

//////////////////////////////////////////////////////////////////////
//
// Field
//
//////////////////////////////////////////////////////////////////////

class Field : public juce::Component,
              public juce::ComboBox::Listener,
              public juce::Label::Listener
{
  public:
    
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void fieldChanged(Field* f) = 0;
    };

    enum class Type {Integer, Boolean, String};

    enum class RenderType {Text, Combo, Check, Slider, Rotary, List};
    
    Field(juce::String name, juce::String displayName, Type type);
    Field(const char* name, Type type);
    ~Field();

    void addListener(Listener* l) {
        fieldListener = l;
    }

    void setHelpArea(class HelpArea* ha) {
        helpArea = ha;
    }
        
    const juce::String& getName() {
        return name;
    }

    Type getType() {
        return type;
    }
    
    /**
     * Fields may have an alternative diplay name
     * that is used for the label.
     */
    void setDisplayName(const char* s) {
        displayName = s;
    }

    const juce::String& getDisplayableName() {
        return (displayName.length() > 0) ? displayName : name;
    }

    /**
     * Set the suggested rendering type if you don't want it to default.
     */
    void setRenderType(RenderType rt) {
        renderType = rt;
    }

    void setMulti(bool b) {
        multi = b;
    }

    void setUnmanagedLabel(bool b) {
        unmanagedLabel = b;
    }

    bool isUnManagedLabel() {
        return unmanagedLabel;
    }
    
    /**
     * Integer fields may have a range.
     * If a range is specified the field will be rendered
     * using a knob or slider.  If a range is not specified
     * it is rendered as a text box whose size may be influenced
     * by the size member.
     */
    void setMin(int i) {
        min = i;
    }

    int getMin() {
        return min;
    }

    void setMax(int i) {
        max = i;
    }

    int getMax() {
        return max;
    }

    /**
     * For string fields, this is the number of characters to display.
     */
    void setWidthUnits(int i) {
        widthUnits = i;
    }

    /**
     * For string fields that use a value selection list, the number of
     * lines to display.  The default is the length of the allowed values
     * list up to a maximum of 4.
     */
    void setHeightUnits(int i) {
        heightUnits = i;
    }
    
    /**
     * String values may have value limits.
     * TODO: needs a lot more work to have internal/display values
     * in a proper CC++ way.  Find a tuple class in std:: or something
     */
    juce::StringArray getAllowedValues();

    /**
     * String values with limits may want an alternative name
     * to display for the values.  The size of this array
     * must be the same as the allowed values list.
     */
    juce::StringArray getAllowedValueLabels();

    // work on how we want to consistently define these
    void setAllowedValues(const char** a);
    void setAllowedValues(juce::StringArray& src);
    
    void setAllowedValueLabels(const char** a);
    void setAllowedValueLabels(juce::StringArray& src);

    void setValue(juce::var value);
    juce::var getValue();
    int getIntValue();
    juce::String getStringValue();
    const char* getCharValue();
    bool getBoolValue();

    void addAnnotation(int width);
    void setAnnotation(juce::String text);

    // build out the Juce components to display this field
    void render();
    juce::Rectangle<int> getMinimumSize();
    int getLabelWidth();
    int getRenderWidth();
    
    //
    // Juce interface
    //

    void resized() override;
    void paint (juce::Graphics& g) override;

    // ComboBox
    void comboBoxChanged(juce::ComboBox* box) override;

    // Label
    void labelTextChanged(juce::Label* l) override;

    // MouseListener
    void mouseEnter(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    
  private:
    
    void initLabel();
    void renderLabel();
    void renderInt();
    void renderString();
    void renderBool();
    void loadValue();
    
    Listener* fieldListener = nullptr;
    class HelpArea* helpArea = nullptr;
    juce::String name;
    juce::String displayName;
    Type type = Type::Integer;
    bool unmanagedLabel = false;
    RenderType renderType = RenderType::Text;
    bool multi = false;
    int min = 0;
    int max = 0;
    int widthUnits = 0;
    int heightUnits = 0;
    
    juce::StringArray allowedValues = {};
    juce::StringArray allowedValueLabels = {};

    // the current value of this field
    juce::var value;

    // rendered components
    
    // readable label, for booleans, we could make use of the built-in
    // labeling of ToggleButton
    juce::Label label;
    
    // component used to render the value, dependent on Type and options
    juce::Label textbox;
    juce::ToggleButton checkbox;
    // horrible kludge for Capture in the binding panels
    juce::Label annotation;
    juce::ComboBox combobox;
    juce::Slider slider;
    SimpleListBox listbox;
    
    // the component we chose to render the value
    juce::Component* renderer = nullptr;
 };

