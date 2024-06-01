/**
 * A form panel for configuring MIDI devices.
 *
 * There isn't much to do here.  If we're running standalone you can
 * configure a single input and output device, and an optional output device
 * for the plugin.
 *
 * If we're running as a plugin you can only configure the output device.
 * 
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/MobiusConfig.h"
#include "../../Supervisor.h"
#include "../../MidiManager.h"

#include "../common/Form.h"
#include "../common/Field.h"
#include "../JuceUtil.h"

#include "ConfigEditor.h"
#include "LogPanel.h"
 
#include "MidiDevicesPanel.h"

/**
 * Setting multi-off so we only show the devices for the local host. 
 */
MidiDevicesPanel::MidiDevicesPanel(ConfigEditor* argEditor) :
    ConfigPanel{argEditor, "MIDI Devices", ConfigPanelButton::Save | ConfigPanelButton::Cancel, false}
{
    setName("MidiDevicesPanel");

    // don't need help
    setHelpHeight(0);
    render();
}

MidiDevicesPanel::~MidiDevicesPanel()
{
    // members will delete themselves
    // remove the MidiManager log if we were still showing
    hiding();
}

//////////////////////////////////////////////////////////////////////
//
// ConfigPanel overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by ConfigEditor when we're about to be made visible.
 * Give our log to MidiManager
 *
 * This is kind of dangerous since MidiManager is a singleton
 * and we could have a limited lifetime, though we don't right now
 * listener model might be better, but it's really about the same
 * as what KeyboardPanel does.
 */
void MidiDevicesPanel::showing()
{
    MidiManager* mm = Supervisor::Instance->getMidiManager();
    mm->addListener(this);
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void MidiDevicesPanel::hiding()
{
    MidiManager* mm = Supervisor::Instance->getMidiManager();
    mm->removeListener(this);
}

/**
 * MidiManager Listener
 */
void MidiDevicesPanel::midiMessage(const juce::MidiMessage& message, juce::String& source)
{
    juce::String msg = source + ": ";

    int size = message.getRawDataSize();
    const juce::uint8* data = message.getRawData();
    for (int i = 0 ; i < size ; i++) {
        msg += juce::String(data[i]) + " ";
    }
    log.add(msg);
}

/**
 * Called by ConfigEditor when asked to edit devices.
 * Unlike most other config panels, we don't have a lot of complex state to manage.
 * We also do not edit the DeviceConfig directly, instead get/set selections through
 * MidiManager which will cause the config to become dirty and flushed on shutdown.
 */
void MidiDevicesPanel::load()
{
    if (!loaded) {

        MidiManager* mm = Supervisor::Instance->getMidiManager();

        juce::String input = mm->getInput();
        juce::String output = mm->getOutput();
        juce::String pluginOutput = mm->getPluginOutput();

        if (Supervisor::Instance->isPlugin()) {
            pluginOutputField->setValue(pluginOutput);
        }
        else {
            inputField->setValue(input);
            outputField->setValue(output);
            pluginOutputField->setValue(pluginOutput);
        }
        
        loaded = true;
        // force this true for testing
        changed = true;
    }
}

/**
 * Called by the Save button in the footer.
 * Tell the ConfigEditor we are done.
 *
 * We don't actually have pending state to save, the field listeners
 * directly modified the devices.  I suppose we could have saved
 * the original values and restore them if they click Cancel.
 *
 * Don't need to save devices.xml now since Supervisor will do that
 * automatically on shutdown, but go ahead so we can see the results
 * immediately.
 */
void MidiDevicesPanel::save()
{
    if (changed) {
        
        Supervisor::Instance->updateDeviceConfig();

        loaded = false;
        changed = false;
    }
    else if (loaded) {
        loaded = false;
    }
}

/**
 * Throw away all editing state.
 */
void MidiDevicesPanel::cancel()
{
    loaded = false;
    changed = false;
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * MidiDevicesContent is a wrapper around the Form used to select
 * devices and a LogPanel used to display MIDI events.  Necessary
 * because ConfigPanel only allows a single child of it's content
 * component and we want to control layout of the form relative
 * to the log.
 *
 * Interesting component ownership problem...
 * I all I want this to do is handle the layout but I'd like the
 * components owned by the parent, at least the LogPanel.  In resized
 * we either have to have it make assumptions about the children
 * or have the parent give it concrete references to them.   That's
 * like how Form works.  Think about a good pattern for this if
 * it happens more.
 */
void MidiDevicesContent::resized()
{
    // the form will have sized itself to the minimum bounds
    // necessary for the fields
    // leave a little gap then let the log window fill the rest
    // of the available space
    juce::Rectangle<int> area = getLocalBounds();
    
    // kludge, work out parenting awareness
    Form* form = (Form*)getChildComponent(0);
    if (form != nullptr) {
        juce::Rectangle<int> min = form->getMinimumSize();
        form->setBounds(area.removeFromTop(min.getHeight()));
    }
    
    // gap
    area.removeFromTop(20);

    LogPanel* log = (LogPanel*)getChildComponent(1);
    if (log != nullptr)
      log->setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// Form Rendering
//
//////////////////////////////////////////////////////////////////////

void MidiDevicesPanel::render()
{
    initForm();
    form.render();

    mdcontent.addAndMakeVisible(form);
    mdcontent.addAndMakeVisible(log);

    // place it in the ConfigPanel content panel
    content.addAndMakeVisible(mdcontent);

    // have been keeping the same size for all ConfigPanels
    // rather than having them shrink to fit, should move this
    // to ConfigPanel or ConfigEditor
    setSize (900, 600);
}

const char* NoDeviceSelected = "[No Device]";

/**
 * Display all three fields if we're a standalone application,
 * otherwise just the plugin output field.
 */
void MidiDevicesPanel::initForm()
{
    bool plugin = Supervisor::Instance->isPlugin();

    if (!plugin) {
        inputField = new Field("Input Device", Field::Type::String);
        form.add(inputField);

        outputField = new Field("Output Device", Field::Type::String);
        form.add(outputField);
    }

    pluginOutputField = new Field("Plugin Output Device", Field::Type::String);
    form.add(pluginOutputField);
    
    MidiManager* mm = Supervisor::Instance->getMidiManager();
    
    juce::StringArray inputs = mm->getInputDevices();
    inputs.insert(0, juce::String(NoDeviceSelected));
    
    juce::StringArray outputs = mm->getOutputDevices();
    outputs.insert(0, juce::String(NoDeviceSelected));

    if (inputField != nullptr) {
        inputField->setAllowedValues(inputs);
        inputField->addListener(this);
    }

    if (outputField != nullptr) {
        outputField->setAllowedValues(outputs);
        outputField->addListener(this);
    }

    if (pluginOutputField != nullptr) {
        pluginOutputField->setAllowedValues(outputs);
        pluginOutputField->addListener(this);
    }
}

/**
 * Get notifications during initialization before it is even
 * shown.  Not sure why, I think I asked for dontSendNotifications
 */
void MidiDevicesPanel::fieldChanged(Field* field)
{
    if (isVisible()) {
        MidiManager* mm = Supervisor::Instance->getMidiManager();
        juce::String name = field->getStringValue();

        // collapse this to empty string so MidiManager will no to close it
        if (name == NoDeviceSelected) name = "";
    
        if (field == inputField)
          mm->setInput(name);
        else if (field == outputField)
          mm->setOutput(name);
        else if (field == pluginOutputField)
          mm->setPluginOutput(name);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
