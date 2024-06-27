/**
 * Gradual replacement for the old ConfigPanel with some structural
 * provements.
 *
 * The ConfigPanel extends BasePanel which gives it a title bar and footer buttons.
 *
 * In the center is a ContentPanel that may contain an ObjectSelector and a subclass
 * specific content area.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../BasePanel.h"
#include "../common/HelpArea.h"

#include "ConfigEditor.h"

/**
 * The object selector presents a combobox to select one of a list
 * of objects.  It also displays the name of the selected object
 * for editing.   There is a set of buttons for acting on the object list.
 */
class NewObjectSelector : public juce::Component, juce::Button::Listener, juce::ComboBox::Listener
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

/**
 * This is the BasePanel content component.
 * We add the ObjectSelector and HelpArea, both optional.
 * The subclass gives us another level of arbitrary content
 * to put between them.
 */
class ConfigPanelContent : public juce::Component
{
  public:

    ConfigPanelContent();
    ~ConfigPanelContent();

    void setContent(juce::Component* c);
    void enableObjectSelector(NewObjectSelector::Listener* l);
    void setHelpHeight(int height);
    void resized() override;

  private:

    NewObjectSelector objectSelector;
    bool objectSelectorEnabled = false;
    HelpArea helpArea;
    int helpHeight = 0;
    juce::Component* content = nullptr;
};

/**
 * ConfigPanel arranges the previous generic components and
 * holds object-specific component within the content panel.
 * 
 * It is subclassed by the various configuration panels.
 */
class NewConfigPanel : public BasePanel, public NewObjectSelector::Listener
{
  public:

    enum ButtonType {
        // read-only informational panels will have an Ok rather than a Save button
        Save = 2,
        Cancel = 4,
        Revert = 8
    };

    NewConfigPanel(class ConfigEditor* argEditor, const char* titleText, int buttons, bool multi);
    ~NewConfigPanel() override;

    // called by ConfigEditor when the panel is to become visible or hidden
    // in case it needs to make preparations
    virtual void showing() {
    }
    virtual void hiding() {
    }
    
    bool isLoaded() {
        return loaded;
    }

    bool isChanged() {
        return changed;
    }

    // common initialization before a subclass is loaded
    void prepare();
    
    // Subclass overloads

    // prepare for this panel to be shown
    virtual void load() = 0;

    // save the all edited objects and prepare to close
    virtual void save() = 0;

    // throw away any changes and prepare to close
    // is this necessary?  could just implement this as revert
    // for all of them
    virtual void cancel() = 0;

    // respond to ObjectSelector buttons if it supports multiple
    virtual void objectSelectorSelect(int /*ordinal*/) {};
    virtual void objectSelectorNew() {};
    virtual void objectSelectorDelete() {};
    virtual void objectSelectorCopy() {};
    virtual void objectSelectorRename(juce::String /*newName*/) {};

  protected:
    
    class ConfigEditor* editor;
    ConfigPanelContent content;
    
    // set by this class after handling the first prepare() call
    bool prepared = false;

    // set by the subclass if state has been loaded
    bool loaded = false;
    
    // set by the subclass if it was shown and there are pending changes
    bool changed = false;
    
  private:

    bool hasObjectSelector = false;;

    juce::TextButton saveButton;
    juce::TextButton cancelButton;
    juce::TextButton revertButton;

};
