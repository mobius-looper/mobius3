/**
 * The base class of fields that may be arranged in a YanForm.
 * Fields normally have a label that will be rendered by the form.
 * Field subclasses add extra widgetry.
 */

#pragma once

#include <JuceHeader.h>

#include "ColorPopup.h"
#include "../../Services.h"

/**
 * Subclass used for Labels within a YanField/YanForm.
 * Used to capture mouse events for drag-and-drop and
 * special actions like overriding a common value, popup help, etc.
 *
 * Probably better ways to do this.
 *
 * You can only drag a field by clicking on it's label, which
 * is enough for most forms but for those that use rows of adjacent
 * fields with a common label, that won't work.
 *
 * While the mouse listener is on the label, the outside world is
 * notified with the parent YanField.
 */
class YanFieldLabel: public juce::Label
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanFieldClicked(class YanField* f, const juce::MouseEvent& e) = 0;
    };

    // when Drag-and-drop is allowed and initiated, this is the
    // source idtifier prefix put in the description
    constexpr static const char* DragPrefix = "YanField:";
    
    YanFieldLabel(class YanField* parent);

    void setListener(Listener* l);

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
  private:

    Listener* listener = nullptr;

    // the logical parent is the assdociated YanField
    // it is NOT the juce::Component parent
    // YanFrom is allowed to put the label anywhere
    class YanField* parent = nullptr;
    
};

class YanField : public juce::Component
{
  public:
    
    YanField();
    YanField(juce::String argLabel);
    virtual ~YanField();

    void setLabelColor(juce::Colour c);
    void unsetLabelColor();
    void setLabelListener(YanFieldLabel::Listener* l);
    
    void setLabel(juce::String s);
    juce::Label* getLabel();
    
    void setAdjacent(bool b);
    bool isAdjacent();
    virtual bool isSection() {return false;};
    
    int getPreferredWidth(int rowHeight);
    
    virtual int getPreferredComponentWidth() = 0;
    virtual int getPreferredComponentHeight() {return 0;}
    
    virtual void setDisabled(bool b);
    virtual bool isDisabled();
    
    // special support for draggable forms
    void setDragDescription(juce::String s);
    juce::String getDragDescription();

    void setTooltip(juce::String tt);

    void setOrdinal(int i);
    int getOrdinal();
    
  protected:
    
    juce::Rectangle<int> resizeLabel();
        
  private:

    YanFieldLabel label {this};
    bool adjacent = false;
    bool disabled = false;
    int ordinal = 0;
    
    juce::String dragDescription;
    juce::Colour explicitLabelColor;

    void setNormalLabelColor();
};

class YanSpacer : public YanField
{
  public:

    YanSpacer();
    ~YanSpacer();

    int getPreferredComponentWidth() override;
};

/**
 * I suppose it would also work if this were just a Spacer with a label?
 * Fewer moving parts...
 */
class YanSection : public YanField
{
  public:

    YanSection(juce::String label);
    ~YanSection();

    int getPreferredComponentWidth() override;
    bool isSection() override {return true;}
};

class YanInput : public YanField, public juce::Label::Listener, public juce::TextEditor::Listener
{
  public:
    
    YanInput(juce::String label, int charWidth = 0, bool readOnly = false);
    ~YanInput() {}

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void yanInputChanged(YanInput*) {}
        virtual void yanInputEditorShown(YanInput*) {}
        virtual void yanInputEditorHidden(YanInput*) {}
        virtual void yanInputEditorChanged(YanInput*, juce::String) {}
    };
    
    void setListener(Listener* l);
    void setDisabled(bool b) override;

    void setNoBorder(bool b);
    void setBackgroundColor(juce::Colour c);
    
    int getPreferredComponentWidth() override;

    void setValue(juce::String s);
    juce::String getValue();
    void setAndNotify(juce::String s);

    int getInt();
    void setInt(int i);

    void resized() override;
    void paint(juce::Graphics& g) override;
    
    void labelTextChanged(juce::Label* l) override;
    void textEditorTextChanged(juce::TextEditor& ed) override;

  private:

    Listener* listener = nullptr;
    juce::Label text;
    int charWidth = 0;
    bool readOnly = false;
    bool noBorder = false;
};

class YanFile : public YanField, public juce::Button::Listener, public FileChooserService::Handler
{
  public:

    YanFile(juce::String label);
    ~YanFile();
    
    void initialize(juce::String purpose, FileChooserService* svc);
    
    int getPreferredComponentWidth() override;
    
    void resized() override;
    
    // Button::Listener
    void buttonClicked(juce::Button* b) override;

    // FileChooserService::Handler
    void fileChooserResponse(juce::File f) override;
    
  private:

    juce::String purpose;
    FileChooserService* fileChooser = nullptr;
    YanInput input;
    juce::TextButton button;
    
};

class YanCheckbox : public YanField
{
  public:
    
    YanCheckbox(juce::String label);
    ~YanCheckbox() {}

    void setDisabled(bool b) override;
    int getPreferredComponentWidth() override;
    
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
        virtual void yanColorSelected(int argb) = 0;
    };

    YanColorChooser(juce::String label);
    ~YanColorChooser() {}

    void setListener(Listener* l) {
        listener = l;
    }

    void setValue(int i);
    int getValue();

    int getPreferredComponentWidth() override;
    
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
        virtual void yanRadioSelected(YanRadio* r, int selection) = 0;
    };
    void setListener(Listener* l);

    void setButtonLabels(juce::StringArray labels);
    void setButtonCount(int count);
    
    void setSelection(int index);
    int getSelection();

    int getPreferredComponentWidth() override;
    
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
        virtual void yanComboSelected(YanCombo* c, int selection) = 0;
    };
    void setListener(Listener* l);

    void setDisabled(bool b) override;
    void setWidthUnits(int units);
    void setItems(juce::StringArray names);
    void setSelection(int index);
    int getSelection();
    juce::String getSelectionText();

    int getPreferredComponentWidth() override;
    
    void resized() override;
    void comboBoxChanged(juce::ComboBox* box) override;
    
  private:

    Listener* listener = nullptr;
    int initialSelection = -1;
    int widthUnits = 0;

    juce::StringArray items;
    juce::ComboBox combobox;
    
    void setItemNoNotify(int id);
    int calculatePreferredWidth();
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
    
