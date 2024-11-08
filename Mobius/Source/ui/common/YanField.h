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

    void setAdjacent(bool b) {
        adjacent = b;
    }

    bool isAdjacent() {
        return adjacent;
    }
    
    virtual int getPreferredWidth() = 0;
    virtual int getPreferredHeight() { return 0; }

  private:

    bool adjacent = false;

};

class YanSpacer : public YanField
{
  public:

    YanSpacer();
    ~YanSpacer();

    int getPreferredWidth();
};


class YanInput : public YanField, public juce::Label::Listener, public juce::TextEditor::Listener
{
  public:
    
    YanInput(juce::String label, int charWidth = 0, bool readOnly = false);
    ~YanInput() {}

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void inputChanged(YanInput*) {}
        virtual void inputEditorShown(YanInput*) {}
        virtual void inputEditorHidden(YanInput*) {}
        virtual void inputEditorChanged(YanInput*, juce::String) {}
    };
    void setListener(Listener* l);
    
    int getPreferredWidth() override;

    void setValue(juce::String s);
    juce::String getValue();
    void setAndNotify(juce::String s);

    int getInt();
    void setInt(int i);

    void resized() override;
    void labelTextChanged(juce::Label* l) override;
    void textEditorTextChanged(juce::TextEditor& ed) override;

  private:

    Listener* listener = nullptr;
    juce::Label text;
    int charWidth = 0;
    bool readOnly = false;
};

class YanCheckbox : public YanField
{
  public:
    
    YanCheckbox(juce::String label);
    ~YanCheckbox() {}

    int getPreferredWidth() override;
    
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

    int getPreferredWidth() override;
    
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void colorSelected(int argb) override;
    
  protected:

    Listener* listener = nullptr;
    juce::Label text;
    ColorPopup popup;
    int color = 0;
};
  
class YanRadio : public YanField, public juce::Button::Listener
{
  public:

    YanRadio(juce::String label);
    ~YanRadio();

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void radioSelected(YanRadio* r, int selection) = 0;
    };
    void setListener(Listener* l);

    void setButtonLabels(juce::StringArray labels);
    void setButtonCount(int count);
    
    void setSelection(int index);
    int getSelection();

    int getPreferredWidth() override;
    
    void resized() override;
    void buttonClicked(juce::Button* b) override;

  private:

    Listener* listener = nullptr;
    int initialSelection = -1;

    juce::OwnedArray<juce::ToggleButton> buttons;
    
};

class YanCombo : public YanField, public juce::ComboBox::Listener
{
  public:

    YanCombo(juce::String label);
    ~YanCombo();

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void comboSelected(YanCombo* c, int selection) = 0;
    };
    void setListener(Listener* l);

    void setWidthUnits(int units);
    void setItems(juce::StringArray names);
    void setSelection(int index);
    int getSelection();
    juce::String getSelectionText();

    int getPreferredWidth() override;
    
    void resized() override;
    void comboBoxChanged(juce::ComboBox* box) override;
    
  private:

    Listener* listener = nullptr;
    int initialSelection = -1;
    int widthUnits = 0;
    juce::ComboBox combobox;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
    
