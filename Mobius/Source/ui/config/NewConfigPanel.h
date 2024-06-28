/**
 * Gradual replacement for the old ConfigEditor/ConfigPanel with structural improvements.
 *
 * A ConfigPanel surrounds a ConfigEditor and provides common UI componentry
 * such as a dragable panel with a title bar, a row of footer buttons at the bottom,
 * an optional object selector when editing configuration types with multiple
 * objects (Setups, Presets), and a help area to display details about input fields
 * within the editor.
 *
 * ConfigPanel implements ConfigContext which defines the interface the ConfigEditor
 * uses to request UI adjustments and to access to the environment such as reading
 * and saving files.
 *
 * Currently the only ConfigContext implementation is ConfigPanel which extends
 * BasePanel so it can be managed by PanelFactory.  I'm not entirely happy with the
 * way things are glued together here, with multiple levels of "content" objects and
 * subclassing.  Content nesting is necessary due to the way top-down resized()
 * layouts work.  The biggest mess is who gets to be the ConfigEditorContext,
 * is it the outer ConfigPanel (which is a BasePanel subclass) or is it the
 * inner ConfigEditorWrapper.
 *
 * Leaning toward making the Wrapper dumb and doing nothing but providing
 * the resized() layout logic.  Interaction between the ConfigEditor and
 * the outside world is handled by ConfigPanel.
 */

#pragma once

#include <JuceHeader.h>

#include "../BasePanel.h"
#include "../common/HelpArea.h"

// also includes ConfigEditorContxt
#include "NewConfigEditor.h"

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

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void objectSelectorSelect(int ordinal) = 0;
        virtual void objectSelectorRename(juce::String newName) = 0;
        virtual void objectSelectorNew(juce::String newName) = 0;
        virtual void objectSelectorDelete() = 0;
        virtual void objectSelectorCopy() = 0;
    };

    /**
     * The starting name to use for new objects.
     */
    const char* NewName = "[New]";
    
    NewObjectSelector();
    ~NewObjectSelector() override;
    
    void setListener(Listener* l) {
        listener = l;
    }

    void resized() override;
    void paint (juce::Graphics& g) override;

    int getPreferredHeight();

    // set the full list of names to display in the combo box
    void setObjectNames(juce::StringArray names);
    // add a name to the end
    void addObjectName(juce::String name);
    // change the selected name
    void setSelectedObject(int ordinal);
    // get the currently selected name
    juce::String getObjectName();
    // get the currently selected ordinal
    int getObjectOrdinal();
    
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
// ConfigEditorWrapper
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the BasePanel content component and provides a wrapper
 * around a ConfigEditor which is where most of the work gets done.
 * 
 * Here we surround the ConfigEditor with an optional ObjectSelector and HelpArea.
 */
class ConfigEditorWrapper : public juce::Component, public NewObjectSelector::Listener
{
  public:

    ConfigEditorWrapper();
    ~ConfigEditorWrapper();

    // set the inner editor
    void setEditor(NewConfigEditor* argEditor);
    NewConfigEditor* getEditor() {
        return editor;
    }

    // enable the object selector
    // the wrapper will do the listening and forward to the editor
    void enableObjectSelector();
    NewObjectSelector* getObjectSelector() {
        return &objectSelector;
    }
    
    // enable the help area
    void enableHelp(class HelpCatalog* catalog, int height);
    HelpArea* getHelpArea() {
        return &helpArea;
    }
    
    // what all this wrapper mess is for
    void resized() override;

    // ObjectSelector::Listener
    void objectSelectorSelect(int ordinal) override;
    void objectSelectorRename(juce::String newName) override;
    void objectSelectorNew(juce::String newName) override;
    void objectSelectorDelete() override;
    void objectSelectorCopy() override;

  private:

    NewConfigEditor* editor = nullptr;
    
    NewObjectSelector objectSelector;
    bool objectSelectorEnabled = false;
    
    HelpArea helpArea;
    int helpHeight = 0;
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
 * The subclass will call setEditor to give this a ConfigEditor.  See commentary
 * above the setEditor() method for the subtle mess we have here.
 */
class NewConfigPanel : public BasePanel, public ConfigEditorContext
{
  public:

    NewConfigPanel();
    ~NewConfigPanel() override;

    // called by the subclass during construction
    void setEditor(NewConfigEditor* editor);

    // ConfigEditorContext methods called by the ConfigEditor
    // during construction

    void enableObjectSelector() override;
    void enableHelp(int height) override;
    void enableRevert() override;

    class HelpArea* getHelpArea();

    // ConfigEditorContext methods called by the ConfigEditor
    // at runtime

    void setObjectNames(juce::StringArray names) override;
    void addObjectName(juce::String name) override;
    void setSelectedObject(int ordinal) override;
    int getSelectedObject() override;
    juce::String getSelectedObjectName() override;
    
    class MobiusConfig* getMobiusConfig() override;
    void saveMobiusConfig() override;

    class UIConfig* getUIConfig() override;
    void saveUIConfig() override;
    
    class DeviceConfig* getDeviceConfig() override;
    void saveDeviceConfig() override;

    class Supervisor* getSupervisor() override;
    
    // BasePanel overloads
    void showing() override;
    void hiding() override;

    // BasePanel button handler
    void footerButton(juce::Button* b) override;
    
  private:

    ConfigEditorWrapper wrapper;

    juce::TextButton saveButton {"Save"};
    juce::TextButton cancelButton {"Cancel"};
    juce::TextButton revertButton {"Revert"};

    bool loaded = false;
};
