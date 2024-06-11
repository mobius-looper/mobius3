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
#include "../../model/DeviceConfig.h"
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

    // have to defer this post-construction
    inputTable.init(false);
    outputTable.init(true);
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
 *
 * yeah well, that's about to change...
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

        // evolving new style
        DeviceConfig* config = Supervisor::Instance->getDeviceConfig();
        inputTable.load(config);
        outputTable.load(config);
        
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

        DeviceConfig* config = Supervisor::Instance->getDeviceConfig();
        inputTable.save(config);
        outputTable.save(config);
        
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

    // this sucks so hard
    BasicTabs* tabs = (BasicTabs*)getChildComponent(2);
    tabs->setBounds(area.removeFromTop(200));

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
    mdcontent.addAndMakeVisible(tabs);

    tabs.add("Input Devices", &inputTable);
    tabs.add("Output Devices", &outputTable);
    
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

//////////////////////////////////////////////////////////////////////
//
// MidiDeviceTable
//
//////////////////////////////////////////////////////////////////////

MidiDeviceTable::MidiDeviceTable()
{
}

/**
 * Load the available devices into the table
 */
void MidiDeviceTable::init(bool output)
{
    if (!initialized) {
        isOutput = output;
        if (isOutput) {
            addColumn("Name", 1);
            addColumnCheckbox("Export", 2);
            addColumnCheckbox("Sync", 3);
            addColumnCheckbox("Plugin Export", 4);
            addColumnCheckbox("plugin Sync", 5);
        }
        else {
            addColumn("Name", 1);
            addColumnCheckbox("Control", 2);
            addColumnCheckbox("Sync", 3);
            addColumnCheckbox("Plugin Control", 4);
            addColumnCheckbox("plugin Sync", 5);
        }
    
        MidiManager* mm = Supervisor::Instance->getMidiManager();
        juce::StringArray deviceNames;
        if (isOutput)
          deviceNames = mm->getOutputDevices();
        else
          deviceNames = mm->getInputDevices();

        for (auto name : deviceNames) {
            MidiDeviceTableRow* device = new MidiDeviceTableRow();
            device->name = name;
            devices.add(device);
        }

        initialized = true;
    }
}

/**
 * Look up a device in the table by name.
 */
MidiDeviceTableRow* MidiDeviceTable::getRow(juce::String name)
{
    MidiDeviceTableRow* found = nullptr;
    for (auto row : devices) {
        if (row->name == name) {
            found = row;
            break;
        }
    }
    return found;
}

/**
 * Load the state of the current machine's MIDI device selections
 * into the table.
 *
 * The MachineConfig model is not tabular, but it would be a lot cooler
 * if it was.
 *
 * It has a set of attributes with device names as CSVs.  Reconsider that.
 *
 * Realistically, there can only be one sync device selected for either
 * input or output.  I suppose we could allow multiple input sync devices
 * and just assume that only one will be active at a time.  But more than
 * one output sync device doesn't seem useful.  If you really want to
 * send clocks to multiple devices should be using a hub.
 */
void MidiDeviceTable::load(DeviceConfig* config)
{
    MachineConfig* mconfig = config->getMachineConfig();

    if (mconfig != nullptr) {
        if (isOutput) {
            loadDevices(mconfig->midiOutput, false, false);
            loadDevices(mconfig->midiOutputSync, true, false);
            loadDevices(mconfig->pluginMidiOutput, false, true);
            loadDevices(mconfig->pluginMidiOutputSync, true, true);
        }
        else {
            loadDevices(mconfig->midiInput, false, false);
            loadDevices(mconfig->midiInputSync, true, false);
            loadDevices(mconfig->pluginMidiInput, false, true);
            loadDevices(mconfig->pluginMidiInputSync, true, true);
        }
    }
    updateContent();
}

/**
 * Update the table model for a csv of device names.
 */
void MidiDeviceTable::loadDevices(juce::String names, bool sync, bool plugin)
{
    juce::StringArray list = juce::StringArray::fromTokens(names, ",", "");
    for (auto name : list) {
        MidiDeviceTableRow* device = getRow(name);
        if (device == nullptr) {
            // something in the config that was not an active runtime device
            // mark it missing and display highlighted
            device = new MidiDeviceTableRow();
            device->name = name;
            device->missing = true;
            devices.add(device);
        }
        if (sync) {
            if (plugin)
              device->pluginSync = true;
            else
              device->appSync = true;
        }
        else {
            if (plugin)
              device->appControl = true;
            else
              device->appControl = true;
        }
    }
}

void MidiDeviceTable::save(DeviceConfig* config)
{
    (void)config;
    
    // test deriving the csvs from the table but don't save them yet
    juce::String names = getDevices(false, false);
    Trace(2, "MidiDeviceTable: appControl %s\n", names.toUTF8());
    
    names = getDevices(true, false);
    Trace(2, "MidiDeviceTable: appSync %s\n", names.toUTF8());
    
    names = getDevices(false, true);
    Trace(2, "MidiDeviceTable: pluginControl %s\n", names.toUTF8());

    names = getDevices(true, true);
    Trace(2, "MidiDeviceTable: pluginSync %s\n", names.toUTF8());
}

juce::String MidiDeviceTable::getDevices(bool sync, bool plugin)
{
    juce::StringArray list;

    for (auto device : devices) {
        bool include = false;
        if (sync) {
            if (plugin)
              include = device->pluginSync;
            else
              include = device->appSync;
        }
        else {
            if (plugin)
              include = device->pluginControl;
            else
              include = device->appControl;
        }
        if (include)
          list.add(device->name);
    }

    return list.joinIntoString(",");
}

//
// BasicTable::Model
//


int MidiDeviceTable::getNumRows()
{
    return devices.size();
}

juce::String MidiDeviceTable::getCellText(int row, int columnId)
{
    juce::String value;
    
    MidiDeviceTableRow* device = devices[row];
    if (device == nullptr) {
        Trace(1, "MidiDeviceTable::getCellText row out of bounds %d\n", row);
    }
    else if (columnId == 1) {
        value = device->name;
    }
    else {
        // these are all checkboxes, shouldn't be here
        Trace(1, "MidiDeviceTable::getCellText not supposed to be here\n");
    }
    return value;
}

bool MidiDeviceTable::getCellCheck(int row, int column)
{
    bool state = false;
    
    MidiDeviceTableRow* device = devices[row];
    if (device == nullptr) {
        Trace(1, "MidiDeviceTable::getCellCheck row out of bounds %d\n", row);
    }
    else {
        switch (column) {
            case 2: state = device->appControl; break;
            case 3: state = device->appSync; break;
            case 4: state = device->pluginControl; break;
            case 5: state = device->pluginSync; break;
            default:
                Trace(1, "MidiDeviceTable::getCellCheck column id out of bounds %d\n", column);
                break;
        }
    }
    return state;
}

/**
 * Interface right now is proactive, where we receive notificiations
 * when the checkbox changes.  If we don't try to respond to these
 * dynamically then we don't need that and could just let the
 * table do its thing and dig out the checks during save()
 */
void MidiDeviceTable::setCellCheck(int row, int column, bool state)
{
    MidiDeviceTableRow* device = devices[row];
    if (device == nullptr) {
        Trace(1, "MidiDeviceTable::setCellCheck row out of bounds %d\n", row);
    }
    else {
        switch (column) {
            case 2: device->appControl = state; break;
            case 3: device->appSync = state; break;
            case 4: device->pluginControl = state; break;
            case 5: device->pluginSync = state; break;
            default:
                Trace(1, "MidiDeviceTable::setCellCheck column id out of bounds %d\n", column);
                break;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
