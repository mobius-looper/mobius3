/**
 * ConfigEditor for configuring MIDI devices.
 *
 * This can work two ways...
 * 
 * Most of the config editors are Save/Cancel editors.
 * You're editing an object read from a file and nothing happens
 * until you click Save and the object is written back to the file
 * and the changes propagated to the rest of the system.  If you click
 * Cancel nothing is changed and you start over the next time.
 *
 * The AudioEditor doesn't work that way, mostly because the Juce component
 * it uses to show the options has an immediate effect on the running system.
 * We still need to save the changes in a file so they can be restored on the
 * next restart, but there is no Cancel.  I suppose there could be, by restoring
 * the audio settings from the file, but it doesn't do that now.
 *
 * The MidiDevices editor can work either way, it can just edit part of the
 * devices.xml file and nothing happens till you click save.  For this one though,
 * it's nice for it to work like AudioEditor and actually open the devices as you
 * select them so we can watch the log and monitor MIDI messages to see if something
 * is actually comming in as expected.  Without that you would have to Save the editor
 * and close it, bring up MidiMonitor and test it, then bring up the editor again if the
 * wrong device was selected.
 *
 * Now that this has immediate impact on the devices, we don't need Save/Cancel buttons
 * here either.
 *
 * This is different than AudioEditor though because here we ARE directly editing
 * an object read from a file.  So when the editor is closed it needs to update the
 * file.  AudioEditor doesn't do that when you close the editor, instead Supervisor captures
 * the final state of the audio devices at shutdown.
 *
 * Either an on-close or deferred-till-shutdown method works, except thta if you do deferred,
 * when the panel is loaded again it CAN'T reload state from the file like most editors do.
 * It has to leave the open device as it was the last time the user interacted with it.
 * This is possible if you conditionalize load() to only load the file the first time
 * but not the second time, but we still need to look at the runtime state to see what
 * boxes to check.  AudioEditor doesn't have to do this since the editing component is
 * built-in to Juce and already does that.
 *
 * To make AudioEditor and MidiDeviceEditor work similarly, it is easiest to always
 * update devices.xml when the editor is closed AND again on shutdown just in case you
 * shut down without closing the editor panel.
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

#include "MidiDeviceEditor.h"

MidiDeviceEditor::MidiDeviceEditor()
{
    setName("MidiDeviceEditor");
    
    addAndMakeVisible(tabs);
    addAndMakeVisible(log);

    inputTable.setCheckboxListener(this);
    outputTable.setCheckboxListener(this);

    tabs.add("Input Devices", &inputTable);
    tabs.add("Output Devices", &outputTable);
}

MidiDeviceEditor::~MidiDeviceEditor()
{
    // members will delete themselves
    // remove the MidiManager log if we were still showing
    hiding();
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
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
void MidiDeviceEditor::showing()
{
    MidiManager* mm = context->getSupervisor()->getMidiManager();
    mm->addMonitor(this);
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void MidiDeviceEditor::hiding()
{
    MidiManager* mm = context->getSupervisor()->getMidiManager();
    mm->removeMonitor(this);
}

/**
 * MidiManager::Monitor
 */
void MidiDeviceEditor::midiMonitor(const juce::MidiMessage& message, juce::String& source)
{
    log.midiMessage(message, source);
}

bool MidiDeviceEditor::midiMonitorExclusive()
{
    return true;
}

void MidiDeviceEditor::midiMonitorMessage(juce::String msg)
{
    log.add(msg);
}

/**
 * Called by ConfigEditor when asked to edit devices.
 */
void MidiDeviceEditor::load()
{
    // have to defer this post-construction
    MidiManager* mm = context->getSupervisor()->getMidiManager();
    inputTable.init(mm, false);
    outputTable.init(mm, true);

    // pull out the MachineConfig for this machinea
    DeviceConfig* config = context->getDeviceConfig();

    // actively edit the live config
    machine = config->getMachineConfig();
    
    inputTable.load(machine);
    outputTable.load(machine);

    log.showOpen();
}

/**
 * Called by the Save button in the footer.
 * Tell the ConfigEditor we are done.
 */
void MidiDeviceEditor::save()
{
    // don't need this if we've been in live mode
    inputTable.save(machine);
    outputTable.save(machine);
        
    // update the file
    context->saveDeviceConfig();
}

/**
 * Throw away all editing state.
 * As described in the file comments, this doesn't actually cancel,
 * it leaves the devices as they were.  If we need to support actual
 * cancel, then we can't update devices.xml on save(), or we need to keep a copy
 * and re-write the file and re-initializes the devices.
 */
void MidiDeviceEditor::cancel()
{
    MidiManager* mm = context->getSupervisor()->getMidiManager();
    mm->openDevices();
}

void MidiDeviceEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    log.setBounds(area.removeFromBottom(100));
    tabs.setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Called by either the input or output device table when a checkbox
 * is clicked on or off.
 *
 * We don't maintain an active edit of the MachineConfig model, the table itself
 * is the model that will be converted back into MachineConfig on exit.
 *
 * God this is a mess due to the app/plugin dupllication and the input/output table split
 * and how BasicTable has it's own model that is sort of like MidiDevicesRow but different.
 */
void MidiDeviceEditor::tableCheckboxTouched(BasicTable* table, int row, int colid, bool state)
{
    // the device we touched
    MidiDeviceTable* mdt = static_cast<MidiDeviceTable*>(table);
    MidiDeviceTableRow* device = mdt->getRow(row);
    juce::String deviceName = mdt->getName(row);
    bool plugin = context->getSupervisor()->isPlugin();
    MidiDeviceColumn mdcol = (MidiDeviceColumn)colid;
    
    // reflect the state in the DeviceTableRow model
    if (!state) {
        device->checks.removeAllInstancesOf(mdcol);
    }
    else {
        // all but input only allows one
        if (mdcol != MidiColumnInput && mdcol != MidiColumnPluginInput)
          mdt->uncheckOthers(mdcol, row);
        device->checks.add(mdcol);
    }
    
    // reflect the state in the open devices
    MidiManager* mm = context->getSupervisor()->getMidiManager();
    switch (mdcol) {
        case MidiColumnInput: {
            if (!plugin)
              mm->openInput(deviceName, MidiManager::Usage::Input);
        }
            break;
        case MidiColumnInputSync: {
            if (!plugin)
              mm->openInput(deviceName, MidiManager::Usage::InputSync);
        }
            break;
        case MidiColumnOutput: {
            if (!plugin)
              mm->openOutput(deviceName, MidiManager::Usage::Output);
        }
            break;
        case MidiColumnOutputSync: {
            if (!plugin)
              mm->openOutput(deviceName, MidiManager::Usage::OutputSync);
        }
            break;
        case MidiColumnThru: {
            if (!plugin)
              mm->openOutput(deviceName, MidiManager::Usage::Thru);
        }
            break;
            
        case MidiColumnPluginInput: {
            if (plugin)
              mm->openInput(deviceName, MidiManager::Usage::Input);
        }
            break;
        case MidiColumnPluginInputSync: {
            if (plugin)
              mm->openInput(deviceName, MidiManager::Usage::InputSync);
        }
            break;
        case MidiColumnPluginOutput: {
            if (plugin)
              mm->openOutput(deviceName, MidiManager::Usage::Output);
        }
            break;
        case MidiColumnPluginOutputSync: {
            if (plugin)
              mm->openOutput(deviceName, MidiManager::Usage::OutputSync);
        }
            break;
        case MidiColumnPluginThru: {
            if (plugin)
              mm->openOutput(deviceName, MidiManager::Usage::Thru);
        }
            break;
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
void MidiDeviceTable::init(MidiManager* mm, bool output)
{
    if (!initialized) {
        isOutput = output;
        if (isOutput) {
            addColumn("Name", MidiColumnName);
            addColumnCheckbox("App Export", MidiColumnOutput);
            addColumnCheckbox("App Sync", MidiColumnOutputSync);
            addColumnCheckbox("App Thru", MidiColumnThru);
            addColumnCheckbox("Plugin Export", MidiColumnPluginOutput);
            addColumnCheckbox("Plugin Sync", MidiColumnPluginOutputSync);
            addColumnCheckbox("Plugin Thru", MidiColumnPluginThru);
        }
        else {
            addColumn("Name", MidiColumnName);
            addColumnCheckbox("App Control", MidiColumnInput);
            addColumnCheckbox("App Sync", MidiColumnInputSync);
            addColumnCheckbox("Plugin Control", MidiColumnPluginInput);
            addColumnCheckbox("Plugin Sync", MidiColumnPluginInputSync);
        }
    
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
 * MachineConfig model represents each device list as a CSV.
 */
void MidiDeviceTable::load(MachineConfig* config)
{
    if (config != nullptr) {
        if (isOutput) {
            loadDevices(config->midiOutput, MidiColumnOutput);
            loadDevices(config->midiOutputSync, MidiColumnOutputSync);
            loadDevices(config->midiThru, MidiColumnThru);
            loadDevices(config->pluginMidiOutput, MidiColumnPluginOutput);
            loadDevices(config->pluginMidiOutputSync, MidiColumnPluginOutputSync);
            loadDevices(config->pluginMidiThru, MidiColumnPluginThru);
        }
        else {
            loadDevices(config->midiInput, MidiColumnInput);
            loadDevices(config->midiInputSync, MidiColumnInputSync);
            loadDevices(config->pluginMidiInput, MidiColumnPluginInput);
            loadDevices(config->pluginMidiInputSync, MidiColumnPluginInputSync);
        }
    }
    updateContent();
}

/**
 * Update the table model for a csv of device names.
 */
void MidiDeviceTable::loadDevices(juce::String names, MidiDeviceColumn colid)
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
        device->checks.add(colid);
    }
}

void MidiDeviceTable::save(MachineConfig* config)
{
    if (isOutput) {
        config->midiOutput = getDevices(MidiColumnOutput);
        config->midiOutputSync = getDevices(MidiColumnOutputSync);
        config->midiThru = getDevices(MidiColumnThru);
        config->pluginMidiOutput = getDevices(MidiColumnPluginOutput);
        config->pluginMidiOutputSync = getDevices(MidiColumnPluginOutputSync);
        config->pluginMidiThru = getDevices(MidiColumnPluginThru);
    }
    else {
        config->midiInput = getDevices(MidiColumnInput);
        config->midiInputSync = getDevices(MidiColumnInputSync);
        config->pluginMidiInput = getDevices(MidiColumnPluginInput);
        config->pluginMidiInputSync = getDevices(MidiColumnPluginInputSync);
    }
}

juce::String MidiDeviceTable::getDevices(MidiDeviceColumn colid)
{
    juce::StringArray list;

    for (auto device : devices) {
        if (device->checks.contains(colid)) {
            list.add(device->name);
        }
    }

    return list.joinIntoString(",");
}

juce::String MidiDeviceTable::getName(int rownum)
{
    MidiDeviceTableRow* row = devices[rownum];
    return row->name;
}

/**
 * Uncheck a column in the table for all rows except one.
 */
void MidiDeviceTable::uncheckOthers(MidiDeviceColumn colid, int selectedRow)
{
    for (int i = 0 ; i < devices.size() ; i++) {
        MidiDeviceTableRow* row = devices[i];
        if (i != selectedRow)
          row->checks.removeAllInstancesOf(colid);
    }
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
    else if (columnId == MidiColumnName) {
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
    else if (columnId == MidiColumnName) {
        if (device->missing)
          color = juce::Colours::red;
    }
    else {
        // these are all checkboxes, shouldn't be here
        Trace(1, "MidiDeviceTable::getCellText not supposed to be here\n");
    }
    return color;
}

bool MidiDeviceTable::getCellCheck(int row, int columnId)
{
    bool state = false;
    
    MidiDeviceTableRow* device = devices[row];
    if (device == nullptr) {
        Trace(1, "MidiDeviceTable::getCellCheck row out of bounds %d\n", row);
    }
    else {
        state = device->checks.contains((MidiDeviceColumn)columnId);
    }
    return state;
}

/**
 * Interface right now is proactive, where we receive notificiations
 * when the checkbox changes.  If we don't try to respond to these
 * dynamically then we don't need that and could just let the
 * table do its thing and dig out the checks during save()
 */
void MidiDeviceTable::setCellCheck(int row, int columnId, bool state)
{
    MidiDeviceTableRow* device = devices[row];
    if (device == nullptr) {
        Trace(1, "MidiDeviceTable::setCellCheck row out of bounds %d\n", row);
    }
    else if (state) {
        device->checks.add((MidiDeviceColumn)columnId);
    }
    else {
        device->checks.removeAllInstancesOf((MidiDeviceColumn)columnId);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
