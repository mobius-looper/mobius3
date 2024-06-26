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

#include "../common/HelpArea.h"

/**
 * Types of buttons that may be displayed along the bottom.
 * These are or'd together and passed to the constructor.
 */
enum NewConfigPanelButton {
    // read-only informational panels will have an Ok rather than a Save button
    Ok = 1,
    Save = 2,
    Cancel = 4,
    Revert = 8
};

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
        Copy,
        Revert
    };

    ObjectSelector(class ConfigPanel* parent);
    ~ObjectSelector() override;

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

    class ConfigPanel* parentPanel;
    
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

    ContentPanel();
    ~ContentPanel();

    void setContent(juce::Component* c);

    void resized() override;

  private:

    NewObjectSelector objectSelector;
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
class NewConfigPanel : public BasePanel
{
  public:

    ConfigPanel(class ConfigEditor* argEditor, const char* titleText, int buttons, bool multi);
    ~ConfigPanel() override;

    // the subclass provides a content component for the center
    // under ObjectSelect and above HelpArea
    void setContent(juce::Component* c);;
    void setHelpHeight(int h);

    // Component
    void resized() override;
    void paint (juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    
    // callbacks from the object selector
    // could these be pure virtual?
    void selectorButtonClicked(ObjectSelector::ButtonType button);
    void objectSelected(juce::String name);

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
    virtual void selectObject(int /*ordinal*/) {};
    virtual void newObject() {};
    virtual void deleteObject() {};
    virtual void copyObject() {};
    virtual void revertObject() {};
    virtual void renameObject(juce::String /*newName*/) {};

  protected:
    
    class ConfigEditor* editor;
    ConfigContentPanel content;
    
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
