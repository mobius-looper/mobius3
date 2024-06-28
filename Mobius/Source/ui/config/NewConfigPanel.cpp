/**
 * Initialization order is extremely subtle due to subclassing, inline object
 * initialization, and cross references.
 *
 * I'm avoiding doing most initialization in constructors to gain more control
 * and make it obvious when things happen.
 * 
 * The order is this, which is already pretty terrible, but it could be much worse:
 *
 * 1) PanelFactory calls new on a ConfigPanel subclass such as PresetPanel
 *    This is the outermost clean interface and where the mess starts.
 * 
 * 2) PresetPanel has an inline member object PresetEditor
 *    It would be simpler in an number of ways to defer this and dynamically allocate
 *    it but hey, let's do fucking RAII since that's all the rage now.
 * 
 * 3) At this point the PresetEditor and PresetPanel class hierarchy constructors
 *    will all be run in a non-obvious order.  If you know what that is, good for you,
 *    but a lot of people won't and since these are intertwined it's very easy to introduce
 *    dependencies that take hours to figure out.  So the requirement is that
 *    ConfigEditor and it's subclass constructors do nothing.
 *
 * 4) BasePanel is at the root of the PresetPanel hierarchy so it's constructor is called
 *    It adds subcomponents for the title bar and footer buttons.
 *
 * 5) ConfigPanel constructor is called next
 *    It makes modifications to the button list created by BasePanel and installs
 *    the ConfigEditorWrapper object as the content component of BasePanel
 *
 * 6) PresetPanel constructor resumes, and by this point the PresetEditor
 *    member object constructor will have finished.  PresetPanel calls
 *    setEditor on itself passing the PresetEditor.
 *
 * 7) ConfigPanel::setEditor installs PresetEditor inside the wrapper,  and calls
 *    PresetEditor::prepare passing itself as the ConfigEditorContext.
 *
 * 8) PresetEditor::prepare calls back to ConfigPanel to make further adjustments.
 *
 * 9) ConfigPanel::setEditor regains control and sets the final default size.
 *
 * 10) Everyone raises a glass to C++, RAII, Juce top-down resized() layout management,
 *     and me, trying to share common code despite all that.
 *
 * There are almost certainly more obvious ways to structure that mess, but at least
 * the interfacess on the edges at PanelFactory and within ConfigEditor are
 * relatively clean.
 */

#include <JuceHeader.h>

#include "../../Supervisor.h"

#include "NewConfigPanel.h"

//////////////////////////////////////////////////////////////////////
//
// ConfigPanel
//
//////////////////////////////////////////////////////////////////////

NewConfigPanel::NewConfigPanel()
{
    setName("NewConfigPanel");

    // always replace the single "Ok" button from BasePanel
    // with Save/Revert/Cancel
    // todo: make Revert optional, and support custom ones like Capture

    resetButtons();
    addButton(&saveButton);
    addButton(&cancelButton);
    setContent(&wrapper);
}

NewConfigPanel::~NewConfigPanel()
{
}

/**
 * Here is where the magic wand waves.
 */
void NewConfigPanel::setEditor(NewConfigEditor* editor)
{
    // set the BasePanel title 
    setTitle(editor->getTitle());
    
    // put the editor inside the wrapper between the ObjectSelector and HelpArea
    wrapper.setEditor(editor);
    
    // give the editor a handle to the thing that provides access to
    // the outside world, which just happens to be us
    editor->prepare(this);

    // set the starting size only after things are finished wiring up so
    // that the initial resized() pass sees everything
    // it would be nice to allow the subclass to ask for a different size,
    // I guess we could allow the subclass constructor or prepare() do that and just
    // test here to see if the size was already set
    setSize (900, 600);
}

//
// ConfigEditor callbacks to adjust the display
//

void NewConfigPanel::enableObjectSelector() {

    wrapper.enableObjectSelector();
}

void NewConfigPanel::enableHelp(int height)
{
    wrapper.enableHelp(getSupervisor()->getHelpCatalog(), height);
}

HelpArea* NewConfigPanel::getHelpArea()
{
    return wrapper.getHelpArea();
}

void NewConfigPanel::enableRevert()
{
    addButton(&revertButton);
}

void NewConfigPanel::setObjectNames(juce::StringArray names)
{
    wrapper.getObjectSelector()->setObjectNames(names);
}

void NewConfigPanel::addObjectName(juce::String name)
{
    wrapper.getObjectSelector()->addObjectName(name);
}

juce::String NewConfigPanel::getSelectedObjectName()
{
    return wrapper.getObjectSelector()->getObjectName();
}

int NewConfigPanel::getSelectedObject()
{
    return wrapper.getObjectSelector()->getObjectOrdinal();
}

void NewConfigPanel::setSelectedObject(int ordinal)
{
    wrapper.getObjectSelector()->setSelectedObject(ordinal);
}

//
// ConfigEditor callbacks to access files
//

/**
 * Try to stop using Supervisor::Instance so much in code below this.
 * Pass this in the constructor so we don't have to.
 */
Supervisor* NewConfigPanel::getSupervisor()
{
    return Supervisor::Instance;
}

class MobiusConfig* NewConfigPanel::getMobiusConfig()
{
    return getSupervisor()->getMobiusConfig();
}

void NewConfigPanel::saveMobiusConfig()
{
    getSupervisor()->updateMobiusConfig();
}


class UIConfig* NewConfigPanel::getUIConfig()
{
    return getSupervisor()->getUIConfig();
}

void NewConfigPanel::saveUIConfig()
{
    getSupervisor()->updateUIConfig();
}
    
class DeviceConfig* NewConfigPanel::getDeviceConfig()
{
    return getSupervisor()->getDeviceConfig();
}

void NewConfigPanel::saveDeviceConfig()
{
    getSupervisor()->updateDeviceConfig();
}

//
// BasePanel notifications
//

/**
 * Called by BasePanel when we've been invisible, and are now being shown.
 *
 * Here is where we track the loaded state of the editor.  If this is the first
 * time we've ever shown this, or if you want to go back to away to selectively
 * hide/show after they've been loaded, we need to remember load state.
 */
void NewConfigPanel::showing()
{
    NewConfigEditor* editor = wrapper.getEditor();
    
    if (!loaded) {
        editor->load();
        loaded = true;
    }

    editor->showing();
}

/**
 * Making the panel invisible, but this does not cancel load state.
 */
void NewConfigPanel::hiding()
{
    wrapper.getEditor()->hiding();
}

/**
 * Called by BasePanel when a footer button is clicked.
 * Kind of messy forwarding here, should we just let the wrapper
 * deal with this?
 */
void NewConfigPanel::footerButton(juce::Button* b)
{
    if (b == &saveButton) {
        wrapper.getEditor()->save();
        // this resets load
        loaded = false;
    }
    else if (b == &cancelButton) {
        wrapper.getEditor()->cancel();
        // this resets load
        loaded = false;
    }
    else if (b == &revertButton) {
        wrapper.getEditor()->revert();
        // this does not reset load
    }
    else {
        // I guess this would be the place to forward the button to the
        // ConfigPanelContent since it isn't one of the standard ones
        Trace(1, "ConfigPanel: Unsupported button %s\n", b->getButtonText().toUTF8());
    }

    // save and cancel close the panel, revert keeps it going
    if (b == &saveButton || b == &cancelButton)
      close();
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditorWrapper
//
//////////////////////////////////////////////////////////////////////

ConfigEditorWrapper::ConfigEditorWrapper()
{
    helpArea.setBackground(juce::Colours::black);
}

ConfigEditorWrapper::~ConfigEditorWrapper()
{
}

void ConfigEditorWrapper::setEditor(NewConfigEditor* e)
{
    editor = e;
    addAndMakeVisible(e);
}

void ConfigEditorWrapper::enableObjectSelector()
{
    objectSelectorEnabled = true;
    objectSelector.setListener(this);
    addAndMakeVisible(objectSelector);
}

void ConfigEditorWrapper::enableHelp(HelpCatalog* catalog, int height)
{
    helpHeight = height;
    if (helpHeight > 0) {
        addAndMakeVisible(helpArea);

        // NOTE WELL: this is where component object initialization didn't
        // work before we started dynamically allocating ConfigPanelsl
        // Before we were being called during construction, and Supervisor isn't
        // initialized enough to have a HelpCatalog yet to pass down.
        // Now since we create ConfigPanels on demand, Supervisor will have had
        // time to load the catalog.  If that ever changes, then we'll have to
        // go back to using a prepare() phase on the wrapper called during showing()
        // of the ConfigPanel
        helpArea.setCatalog(catalog);
    }
}        

void ConfigEditorWrapper::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    area.removeFromTop(4);
    
    if (objectSelectorEnabled) {
        objectSelector.setBounds(area.removeFromTop(objectSelector.getPreferredHeight()));
        area.removeFromTop(4);
    }

    if (helpHeight > 0) {
        helpArea.setBounds(area.removeFromBottom(helpHeight));
    }

    if (editor != nullptr)
      editor->setBounds(area);
}

// ObjectSelector::Listeners just forward to the wrapped editor

void ConfigEditorWrapper::objectSelectorSelect(int ordinal)
{
    editor->objectSelectorSelect(ordinal);
}

void ConfigEditorWrapper::objectSelectorRename(juce::String newName)
{
    editor->objectSelectorRename(newName);
}

void ConfigEditorWrapper::objectSelectorNew(juce::String newName)
{
    editor->objectSelectorNew(newName);
}

void ConfigEditorWrapper::objectSelectorDelete()
{
    editor->objectSelectorDelete();
}

void ConfigEditorWrapper::objectSelectorCopy()
{
    // never did implement Copy
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector
//
//////////////////////////////////////////////////////////////////////

NewObjectSelector::NewObjectSelector()
{
    setName("ObjectSelector");

    addAndMakeVisible(combobox);
    combobox.addListener(this);
    combobox.setEditableText(true);
    
    addAndMakeVisible(newButton);
    newButton.addListener(this);
    
    addAndMakeVisible(deleteButton);
    deleteButton.addListener(this);

    // todo: have a copyButton, is this still needed?
    // getting implicit copy just by creating a new one
    // might be nice to have an "Init" button instead
}

NewObjectSelector::~NewObjectSelector()
{
}

int NewObjectSelector::getPreferredHeight()
{
    return 30;
}

void NewObjectSelector::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    // todo: calculate max width for object names?
    int comboWidth = 200;
    int comboHeight = 20;

    int centerLeft = (getWidth() - comboWidth) / 2;

    combobox.setBounds(centerLeft, area.getY(), comboWidth, comboHeight);

    newButton.setBounds(combobox.getX() + combobox.getWidth() + 4, area.getY(),
                        30, comboHeight);

    deleteButton.setBounds(newButton.getX() + newButton.getWidth() + 4, area.getY(),
                           50, comboHeight);

}

void NewObjectSelector::paint(juce::Graphics& g)
{
    (void)g;
}

/**
 * Now that we have an editable name, return the name.
 */
juce::String NewObjectSelector::getObjectName()
{
    return combobox.getText();
}

int NewObjectSelector::getObjectOrdinal()
{
    return combobox.getSelectedId() - 1;
}

/**
 * Called by the ConfigPanel subclass to set the names
 * to display in the combobox.
 * When the combobox is changed we call the selectObject overload.
 * This also auto-selects the first name in the list.
 */
void NewObjectSelector::setObjectNames(juce::StringArray names)
{
    combobox.clear();
    // item ids must start from 1
    combobox.addItemList(names, 1);
    combobox.setSelectedId(1, juce::NotificationType::dontSendNotification);
    lastId = 1;
}

/**
 * Juce listener for the object management buttons.
 */
void NewObjectSelector::buttonClicked(juce::Button* b)
{
    if (listener != nullptr) {
        if (b == &newButton) {
            // here we could have some configurability of what
            // we want to the new object name to be, or alternately
            // let the listener return it
            listener->objectSelectorNew(juce::String(NewName));
        }
        else if (b == &deleteButton) {
            listener->objectSelectorDelete();
        }
        else if (b == &copyButton) {
            listener->objectSelectorCopy();
        }
    }
}

/**
 * Carefull here, some of the ComboBox methods use "index"
 * and some use "id".  Index is the zero based array index into
 * the item array, Id is the arbitrary number we can assigned
 * to the item at each index.
 *
 * This is how editable comboboxes seem to work.  If you edit
 * the text displayed in a combobox without using the item
 * selection menu, you get here with SelectedId == 0 and
 * getText returns the text that was entered. The items in
 * the menu do not change, and the checkboxes go away since what
 * is displayed in the text area doesn't match any of the items.
 *
 * If you type in a name that is the same as one of the existing items
 * sometimes it selects the item and sometimes it doesn't.  In my testing
 * I could get the first item selected by typing it's name but not the second.
 *
 * Tutorial on item id 0:
 * " You can use any integer as an item ID except zero. Zero has a special meaning. It
 * is used to indicate that none of the items are selected (either an item hasn't
 * been selected yet or the ComboBox object is displaying some other custom text)."
 *
 * So it kind of becomes a text entry field with a menu glued underneath to auto-fill
 * values.  You are NOT editing the text of an item.  To use this to implement item rename
 * you have to remember the id/index of the last item selected.  When you get
 * selectedId==0 compare the current text to the text of the last selected item and
 * if they are different treat as a rename.
 *
 * You can use escape to abandon the edit.  It appears the only reliable way to
 * have it select an existing item if you type in a matching name is to search and
 * select it in code, this doesn't seem to be automatic.
 * 
 */
void NewObjectSelector::comboBoxChanged(juce::ComboBox* combo)
{
    (void)combo;
    int id = combobox.getSelectedId();
    if (id == 0) {
        juce::String text = combobox.getText();
        juce::String itemText = combobox.getItemText(combobox.indexOfItemId(lastId));
        if (text != itemText) {
            // rename
            if (listener != nullptr)
              listener->objectSelectorRename(text);
            // change the text of the item too
            combobox.changeItemText(lastId, text);
        }
    }
    else {
        // ids are 1 based
        if (listener != nullptr)
          listener->objectSelectorSelect(id - 1);
        lastId = id;
    }
}

void NewObjectSelector::addObjectName(juce::String name)
{
    combobox.addItem(name, combobox.getNumItems() + 1);
}

/**
 * Note well: setSelectedId will by default result
 * in a change notification being sent to the listeners.
 * In this usage, the panel subclasses are managing their
 * own state, and just want to programatically move
 * the selected item. If you change this you need to make
 * sure that the subclass is prepared to immediately receive
 * a selectObject callback as if the user had interacted with
 * the combo box
 * 
 */ 
void NewObjectSelector::setSelectedObject(int ordinal)
{
    combobox.setSelectedId(ordinal + 1, juce::NotificationType::dontSendNotification);
}

// TODO: give the name label a listener to call renameObject

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
