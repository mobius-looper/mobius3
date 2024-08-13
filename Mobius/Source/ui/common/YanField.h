/**
 * The base class of fields that may be arranged in a YanForm.
 * Fields normally have a label that will be rendered by the form.
 * Field subclasses add extra widgetry.
 */

#pragma once

#include <JuceHeader.h>

#include "ColorPopup.h"

class YanField : public juce::Component
{
  public:
    
    YanField(juce::String argLabel) { label = argLabel; }
    virtual ~YanField() {}

    juce::String label;

    virtual int getPreferredWidth() = 0;
    virtual int getPreferredHeight() { return 0; }

};

class YanInput : public YanField
{
  public:
    
    YanInput(juce::String label, int charWidth = 0, bool readOnly = false);
    ~YanInput() {}

    int getPreferredWidth();

    void setValue(juce::String s);
    juce::String getValue();
    void setAndNotify(juce::String s);

    int getInt();
    void setInt(int i);

    void resized() override;

  private:
    
    juce::Label text;
    int charWidth = 0;
    bool readOnly = false;
};

class YanCheckbox : public YanField
{
  public:
    
    YanCheckbox(juce::String label);
    ~YanCheckbox() {}

    int getPreferredWidth();
    
    void setValue(bool b);
    bool getValue();

    void resized() override;
    
  private:
    
    juce::ToggleButton checkbox;
    
};

class YanColorChooser : public YanField, public ColorPopup::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void colorSelected(int argb) = 0;
    };

    YanColorChooser(juce::String label);
    ~YanColorChooser() {}

    void setListener(Listener* l) {
        listener = l;
    }

    void setValue(int i);
    int getValue();

    int getPreferredWidth();
    
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void colorSelected(int argb) override;
    
  protected:

    Listener* listener = nullptr;
    juce::Label text;
    ColorPopup popup;
    int color = 0;
};
  
