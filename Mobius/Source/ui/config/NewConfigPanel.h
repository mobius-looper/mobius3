/**
 * Gradual replacement for the old ConfigPanel with some structural improvements.
 *
 * The ConfigPanel extends BasePanel which gives it a title bar and footer buttons.
 *
 * The BasePanel content component is a ConfigPanelContent that may contain
 * an optional ObjectSelector, an optional HelpArea, and a subclass specific
 * content panel.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../BasePanel.h"
#include "../common/HelpArea.h"

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector
//
//////////////////////////////////////////////////////////////////////

/**
 * The object selector presents a combobox to select one of a list
 * of objects.  It also displays the name of the selected object
 * for editing.   There is a set of buttons for acting on the object list.
 */
class NewObjectSelector : public juce::Component,
                          public juce::Button::Listener,
                          public juce::ComboBox::Listener
{
  public:

    // should we put revert here or in the footer?
    enum ButtonType {
        New,
        Delete,
        Copy
    };

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void objectSelectorSelect(int ordinal) = 0;
        virtual void objectSelectorRename(juce::String newName) = 0;
        virtual void objectSelectorNew() = 0;
        virtual void objectSelectorDelete() = 0;
        virtual void objectSelectorCopy() = 0;
    };
    
    NewObjectSelector();
    ~NewObjectSelector() override;
    
    void setListener(Listener* l) {
        listener = l;
    }

    void resized() override;
    void paint (juce::Graphics& g) override;

    int getPreferredHeight();

    // set the names to display in the combo box
    // currently reserving "[New]" to mean an object that
    // does not yet have a name
    void setObjectNames(juce::StringArray names);
    void addObjectName(juce::String name);
    void setSelectedObject(int ordinal);
    juce::String getObjectName();
    
    // Button Listener
    void buttonClicked(juce::Button* b) override;

    // Combobox Listener
    void comboBoxChanged(juce::ComboBox* combo) override;
  
  private:

    //class ConfigPanel* parentPanel;
    Listener* listener = nullptr;
    juce::ComboBox combobox;
    int lastId = 0;
    
    juce::TextButton newButton {"New"};
    juce::TextButton deleteButton {"Delete"};
    juce::TextButton copyButton {"Copy"};

    // decided to put this in the footer instead
    //juce::TextButton revertButton {"Revert"};
};

//////////////////////////////////////////////////////////////////////
//
// Inner Content
//
//////////////////////////////////////////////////////////////////////

/**
 * Interface of an object that performs the bulk of what this ConfigPanel does.
 * This lives inside the ConfigPanelWrapper between the ObjectSelector and
 * the HelpArea.
 */
class ConfigPanelContent : public juce::Component
{
  public:

    ConfigPanelContent() {}
    virtual ~ConfigPanelContent() {}

    virtual void showing() = 0;
    virtual void hiding() = 0;
    virtual void load() = 0;
    virtual void save() = 0;
    virtual void cancel() = 0;

};

//////////////////////////////////////////////////////////////////////
//
// Wrapper
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the BasePanel content component.
 * We add the ObjectSelector and HelpArea, both optional.
 * The subclass gives us another level of arbitrary content
 * to put between them.
 */
class ConfigPanelWrapper : public juce::Component
{
  public:

    ConfigPanelWrapper();
    ~ConfigPanelWrapper();

    void showing();
    void hiding();

    void setContent(ConfigPanelContent* c);
    void enableObjectSelector(NewObjectSelector::Listener* l);
    void setHelpHeight(int height);
    void resized() override;

  private:

    ConfigPanelContent* content = nullptr;
    
    NewObjectSelector objectSelector;
    bool objectSelectorEnabled = false;
    
    HelpArea helpArea;
    int helpHeight = 0;
    bool prepared = false;
};

//////////////////////////////////////////////////////////////////////
//
// ConfigPanel
//
//////////////////////////////////////////////////////////////////////

/**
 * Extends BasePanel and provides common services to the inner configuration
 * editing component.
 *
 * The subclass constructor will call back to enable the optional features
 * and install the inner content component.
 */
class NewConfigPanel : public BasePanel
{
  public:

    /**
     * BasePanel button overrides
     */
    enum ButtonType {
        Save = 2,
        Cancel = 4,
        Revert = 8
    };

    NewConfigPanel();
    ~NewConfigPanel() override;

    // configuration instructions from the subclass

    void setConfigContent(ConfigPanelContent* c) {
        wrapper.setContent(c);
    }
    
    void enableObjectSelector(NewObjectSelector::Listener* l) {
        wrapper.enableObjectSelector(l);
    }

    void setHelpHeight(int h) {
        wrapper.setHelpHeight(h);
    }

    // BasePanel overloads
    void showing() override;
    void hiding() override;
    
  private:

    ConfigPanelWrapper wrapper;

    juce::TextButton saveButton;
    juce::TextButton cancelButton;
    juce::TextButton revertButton;

};
