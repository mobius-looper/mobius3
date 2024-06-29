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
void NewConfigPanel::setEditor(ConfigEditor* editor)
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
    ConfigEditor* editor = wrapper.getEditor();
    
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

void ConfigEditorWrapper::setEditor(ConfigEditor* e)
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
