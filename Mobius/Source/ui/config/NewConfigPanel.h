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
 * Types of buttons the popup may display at the bottom.
 * These are a bitmask that can be ORd to define the desired buttons.
 */
enum NewConfigPanelButton {
    // read-only informational panels will have an Ok rather than a Save button
    Ok = 1,
    Save = 2,
    Cancel = 4,
    Revert = 8
};

class ConfigPanelFooter : public juce::Component, public juce::Button::Listener
{
  public:
        
    ConfigPanelFooter(class ConfigPanel* parentPanel, int buttons);
    ~ConfigPanelFooter() override;
    
    void resized() override;
    void paint (juce::Graphics& g) override;
    
    int getPreferredHeight();

    // Button::Listener
    virtual void buttonClicked(juce::Button* b) override;
    
  private:
        
    // find a better way to redirect button listeners without needing
    // a back pointer to the parent
    class ConfigPanel* parentPanel;
    int buttonList;
    juce::TextButton okButton;
    juce::TextButton saveButton;
    juce::TextButton cancelButton;
    juce::TextButton revertButton;
    
    void addButton(juce::TextButton* button, const char* text);
};

/**
 * The object selector presents a combobox to select one of a list
 * of objects.  It also displays the name of the selected object
 * for editing.  Is there such a thing as a combo with editable items?
 * There is a set of buttons for acting on the object list.
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

// this wss the wrong way to do this
// change everything to add their own content component
class ConfigContentPanel : public juce::Component
{
  public:

    ContentPanel();
    ~ContentPanel();

    void resized() override;
    void paint (juce::Graphics& g) override;

  private:

    NewObjectSelector objectSelector;
    

};


/**
 * ConfigPanel arranges the previous generic components and
 * holds object-specific component within the content panel.
 * 
 * It is subclassed by the various configuration panels.
 */
class ConfigPanel : public juce::Component
{
  public:


    ConfigPanel(class ConfigEditor* argEditor, const char* titleText, int buttons, bool multi);
    ~ConfigPanel() override;

    // subclass provides a content component for the center
    void setMainContent(juce::Component* c);;
    void setHelpHeight(int h);
    
    void center();

    // Component
    void resized() override;
    void paint (juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    
    // callback from the footer buttons
    void footerButtonClicked(ConfigPanelButton button);
    
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
    ContentPanel content;
    ObjectSelector objectSelector;
    HelpArea helpArea;
    int helpHeight = 0;
    
    // set by this class after handling the first prepare() call
    bool prepared = false;

    // set by the subclass if state has been loaded
    bool loaded = false;
    
    // set by the subclass if it was shown and there are pending changes
    bool changed = false;
    
  private:

    bool hasObjectSelector = false;;
    ConfigPanelHeader header;
    ConfigPanelFooter footer;
    juce::Component* main = nullptr;

    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer dragConstrainer;

};
