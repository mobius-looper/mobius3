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
    
    mdcontent.addAndMakeVisible(tabs);
    mdcontent.addAndMakeVisible(log);

    inputTable.setCheckboxListener(this);
    outputTable.setCheckboxListener(this);

    tabs.add("Input Devices", &inputTable);
    tabs.add("Output Devices", &outputTable);
    
    // place it in the ConfigPanel content panel
    content.addAndMakeVisible(mdcontent);

    // have been keeping the same size for all ConfigPanels
    // rather than having them shrink to fit, should move this
    // to ConfigPanel or ConfigEditor
    setSize (900, 600);
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
    mm->addMonitor(this);

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
    mm->removeMonitor(this);
}

/**
 * MidiManager::Monitor
 */
void MidiDevicesPanel::midiMonitor(const juce::MidiMessage& message, juce::String& source)
{
    log.midiMessage(message, source);
}

bool MidiDevicesPanel::midiMonitorExclusive()
{
    return true;
}

/**
 * Called by ConfigEditor when asked to edit devices.
 */
void MidiDevicesPanel::load()
{
    if (!loaded) {

        DeviceConfig* config = Supervisor::Instance->getDeviceConfig();
        inputTable.load(config);
        outputTable.load(config);

        log.showOpen();
        
        loaded = true;
        // force this true for testing
        changed = true;
    }
}

/**
 * Called by the Save button in the footer.
 * Tell the ConfigEditor we are done.
 */
void MidiDevicesPanel::save()
{
    if (changed) {

        DeviceConfig* config = Supervisor::Instance->getDeviceConfig();
        inputTable.save(config);
        outputTable.save(config);
        
        Supervisor::Instance->updateDeviceConfig();

        // reflect the changes in the active devices
        MidiManager* mm = Supervisor::Instance->getMidiManager();
        mm->openDevices();

        loaded = false;
        changed = false;
    }
    else if (loaded) {
        loaded = false;
    }
}

/**
 * Throw away all editing state.
 *
 * If we ever add an "Apply" button to monitor inputs as they are selected,
 * then cancel would need to restore the open devices to just those
 * that are in the DeviceConfig.
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
 * MidiDevicesContent is a wrapper tabs used to select
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
    juce::Rectangle<int> area = getLocalBounds();
    
    // this sucks so hard
    LogPanel* log = (LogPanel*)getChildComponent(1);
    if (log != nullptr)
      log->setBounds(area.removeFromBottom(100));
    
    BasicTabs* tabs = (BasicTabs*)getChildComponent(0);
    tabs->setBounds(area);
}

/**
 * Called by either the input or output device table when a checkbox
 * is clicked on or off.
 *
 * If the device is relevant to the current runtime context (app vs. plugin)
 * then immediately ask MidiManager to open or close the device, rather than
 * waiting until Save is clicked.  This feels better for the user and is more
 * like how AudioDevicesPanel behaves.  
 */
void MidiDevicesPanel::tableCheckboxTouched(BasicTable* table, int row, int col, bool state)
{
    int colbase = (Supervisor::Instance->isPlugin() ? 4 : 2);
    bool relevant = (col >= colbase && col < colbase + 2);

    bool dynamicOpen = false;

    if (dynamicOpen && relevant) {
        MidiDeviceTable* mdt = static_cast<MidiDeviceTable*>(table);
        juce::String device = mtd->getName(row);
        MidiManager* mm = Supervisor::Instance->getMidiManager();
        
        if (table == &inputTable) {
            // not differentiating between control and sync right now
            if (state)
              mm->openInput(device);
            else
              mm->closeInput(device);
        }
        else {
            if (col > colbase) {
                if (state)
                  mm->openOutputSync(name);
                else
                  mm->closeOutputSync(name);
            }
            else {
                if (state)
                  mm->openOutput(name);
                else
                  mm->closeOutput(name);
            }
        }
    }
        
        log.add("Input " + mdt->getName(row) + " " +
                ((state) ? "opened" : "closed"));
    }
    else {
        log.add("Output " + mdt->getName(row) + " " +
                ((state) ? "opened" : "closed"));
    }
}

//////////////////////////////////////////////////////////////////////
//
// MidiDeviceTable
//
//////////////////////////////////////////////////////////////////////

MidiDeviceTable::MidiDeviceTable()
{
    // are our own model
    setBasicModel(this);
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
            addColumnCheckbox("App Export", 2);
            addColumnCheckbox("App Sync", 3);
            addColumnCheckbox("Plugin Export", 4);
            addColumnCheckbox("Plugin Sync", 5);
        }
        else {
            addColumn("Name", 1);
            addColumnCheckbox("App Control", 2);
            addColumnCheckbox("App Sync", 3);
            addColumnCheckbox("Plugin Control", 4);
            addColumnCheckbox("Plugin Sync", 5);
        }
    
        MidiManager* mm = Supervisor::Instance->getMidiManager();
        juce::StringArray deviceNames;
        if (isOutput)
          deviceNames = mm->getOutputDevices();
        else
          deviceNames = mm->getInputDevices();

        // mioXM piles on a boatload of ports
        // so sort them so you can find things
        deviceNames.sort(false);
        
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
              device->pluginControl = true;
            else
              device->appControl = true;
        }
    }
}

void MidiDeviceTable::save(DeviceConfig* config)
{
    MachineConfig* mconfig = config->getMachineConfig();

    if (isOutput) {
        mconfig->midiOutput = getDevices(false, false);
        mconfig->midiOutputSync = getDevices(true, false);
        mconfig->pluginMidiOutput = getDevices(false, true);
        mconfig->pluginMidiOutputSync = getDevices(true, true);
    }
    else {
        mconfig->midiInput = getDevices(false, false);
        mconfig->midiInputSync = getDevices(true, false);
        mconfig->pluginMidiInput = getDevices(false, true);
        mconfig->pluginMidiInputSync = getDevices(true, true);
    }
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

juce::String MidiDeviceTable::getName(int rownum)
{
    MidiDeviceTableRow* row = devices[rownum];
    return row->name;
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

juce::Colour MidiDeviceTable::getCellColor(int row, int columnId)
{
    juce::Colour color (0);
    
    MidiDeviceTableRow* device = devices[row];
    if (device == nullptr) {
        Trace(1, "MidiDeviceTable::getCellText row out of bounds %d\n", row);
    }
    else if (columnId == 1) {
        if (device->missing)
          color = juce::Colours::red;
    }
    else {
        // these are all checkboxes, shouldn't be here
        Trace(1, "MidiDeviceTable::getCellText not supposed to be here\n");
    }
    return color;
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
