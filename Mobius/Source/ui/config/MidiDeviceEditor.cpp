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
 * What the boxes mean...
 *
 * This was originally more restrictive about multiple devices being open at a time.
 * With the introduction of MidiTracks it is more relaxed and some of the logic might
 * be confusing.   Basically you can have any number of input and output devices selected.
 *
 * In addition you can select inputs and outputs for a paricular "usage".  This is what the
 * system uses to determine which one of several devices is to be used for some purpose
 * like MIDI export, synchronization, or Thru.  For a given usage there can only be one selection.
 *
 * What is different is that before there could only be a single output selected for any purpose,
 * now there can be multiples.  The code also makes it look like an input/output can be deselected
 * but selected for a particular use.  That isn't true, if you select any usage, it is always selected
 * for general use.
 *
 * It might be clearer to skip the checkbox wall and instead just allow selecting multiple
 * input and output devices.  Then for usage provide a combo box to select which of the opened
 * device should be used for that purpose.
 *
 * Because the rules for device dependency are not simple, don't try to enforce them here.
 * As we open/close devices rebuild the table to reflect what the MidiManager actually has.
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

MidiDeviceEditor::MidiDeviceEditor(Supervisor* s) : ConfigEditor(s), log(s)
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
    MidiManager* mm = supervisor->getMidiManager();
    mm->addMonitor(this);
}

/**
 * Called by ConfigEditor when we're about to be made invisible.
 */
void MidiDeviceEditor::hiding()
{
    MidiManager* mm = supervisor->getMidiManager();
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
    MidiManager* mm = supervisor->getMidiManager();
    inputTable.init(mm, false);
    outputTable.init(mm, true);

    // pull out the MachineConfig for this machinea
    DeviceConfig* config = supervisor->getDeviceConfig();
    MachineConfig* machine = config->getMachineConfig();
    
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
    // put table state back into the MachineConfig
    DeviceConfig* config = supervisor->getDeviceConfig();
    MachineConfig* machine = config->getMachineConfig();
    inputTable.save(machine);
    outputTable.save(machine);
        
    // update the file
    supervisor->updateDeviceConfig();
}

/**
 * Throw away all editing state.
 * As described in the file comments, this doesn't actually cancel,
 * it leaves the devices as they were.  If we need to support actual
 * cancel, then we would have to ask MidiManager to reconcile the open devices
 * with what was left in DeviceConfig at the beginning.
 */
void MidiDeviceEditor::cancel()
{
    MidiManager* mm = supervisor->getMidiManager();
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
 * The MidiDeviceTableRow checks array will already have been updated
 * by setCellCheck to have the change, here we can add side effects like
 * unckecking other boxes if only one may be selected in the column, or
 * actively opening/closing the MIDI devices as they are checked.
 *
 * God this is a mess due to the app/plugin duplication and the input/output table split.
 */
void MidiDeviceEditor::tableCheckboxTouched(BasicTable* table, int row, int colid, bool state)
{
    // the device we touched
    MidiDeviceTable* mdt = static_cast<MidiDeviceTable*>(table);
    MidiDeviceTableRow* device = mdt->getRow(row);
    bool plugin = supervisor->isPlugin();
    MidiDeviceColumn mdcol = (MidiDeviceColumn)colid;
    
    // reflect the state in the DeviceTableRow model
    if (state) {
        // when turning on a checkbox, all columns except the primary input/output
        // devices are mutually exclusive
        // todo: it would be better if we let MidiManager do what it does and just
        // reload the tables to reflect that state, but that requires a load() variant
        // that operates from MidiManager rather than DeviceConfig since DeviceConfig
        // hasn't been updated yet
        // OR just always use MidiManager and assume it is following DeviceConfig
        // probelem is that MidiManager only uses half of the config, either app or plugin
        // and expects editor to manage the other half
        if (mdcol != MidiColumnInput && mdcol != MidiColumnPluginInput &&
            mdcol != MidiColumnOutput && mdcol != MidiColumnPluginOutput) {
            
            mdt->uncheckOthers(mdcol, row);
            
            // checking any of these forces the device on the main list
            if (mdcol == MidiColumnInputSync)
              mdt->forceCheck(MidiColumnInput, row);

            else if (mdcol == MidiColumnPluginInputSync)
              mdt->forceCheck(MidiColumnPluginInput, row);
              
            else if (mdcol == MidiColumnExport || mdcol == MidiColumnOutputSync || mdcol == MidiColumnThru)
              mdt->forceCheck(MidiColumnOutput, row);
            
            else 
              mdt->forceCheck(MidiColumnPluginOutput, row);
        }
    }
    else {
        // when turning off a checkbox, if this is one if the primary devices then
        // the usages also all turn off
        if (mdcol == MidiColumnInput) {
            mdt->forceUncheck(MidiColumnInputSync, row);
        }
        else if (mdcol == MidiColumnPluginInput) {
            mdt->forceUncheck(MidiColumnPluginInputSync, row);
        }
        if (mdcol == MidiColumnOutput) {
            mdt->forceUncheck(MidiColumnExport, row);
            mdt->forceUncheck(MidiColumnOutputSync, row);
            mdt->forceUncheck(MidiColumnThru, row);
        }
        else if (mdcol == MidiColumnPluginOutput) {
            mdt->forceUncheck(MidiColumnPluginExport, row);
            mdt->forceUncheck(MidiColumnPluginOutputSync, row);
            mdt->forceUncheck(MidiColumnPluginThru, row);
        }
    }
    
    // reflect the state in the open devices
    // first determine whether this is an input or output device and
    // it's usage
    bool doit = false;
    bool output = false;
    // don't have a Usage::None, so I guess default to Input
    MidiManager::Usage usage = MidiManager::Usage::Input;
    
    switch (mdcol) {
        case MidiColumnInput: {
            usage = MidiManager::Usage::Input;
            doit = !plugin;
        }
            break;
        case MidiColumnInputSync: {
            usage = MidiManager::Usage::InputSync;
            doit = !plugin;
        }
            break;
        case MidiColumnOutput: {
            usage = MidiManager::Usage::Output;
            output = true;
            doit = !plugin;
        }
            break;
        case MidiColumnExport: {
            usage = MidiManager::Usage::Export;
            output = true;
            doit = !plugin;
        }
            break;
        case MidiColumnOutputSync: {
            usage = MidiManager::Usage::OutputSync;
            output = true;
            doit = !plugin;
        }
            break;
        case MidiColumnThru: {
            usage = MidiManager::Usage::Thru;
            output = true;
            doit = !plugin;
        }
            break;
            
        case MidiColumnPluginInput: {
            usage = MidiManager::Usage::Input;
            doit = plugin;
        }
            break;
        case MidiColumnPluginInputSync: {
            usage = MidiManager::Usage::InputSync;
            doit = plugin;
        }
            break;
        case MidiColumnPluginOutput: {
            usage = MidiManager::Usage::Output;
            output = true;
            doit = plugin;
        }
            break;
        case MidiColumnPluginExport: {
            usage = MidiManager::Usage::Export;
            output = true;
            doit = plugin;
        }
            break;
        case MidiColumnPluginOutputSync: {
            usage = MidiManager::Usage::OutputSync;
            output = true;
            doit = plugin;
        }
            break;
        case MidiColumnPluginThru: {
            usage = MidiManager::Usage::Thru;
            output = true;
            doit = plugin;
        }
            break;
            
        default: {
            // a checkbox was added that wasn't handled in the switch
            // leave it as Input, but this will be wrong...
            Trace(1, "MidiDevicEditor: Checkbox handling error\n");
            break;
        }
    }

    // now open/close the device with the derived usage
    MidiManager* mm = supervisor->getMidiManager();
    if (doit) {
        if (output) {
            if (state)
              mm->openOutput(device->name, usage);
            else
              mm->closeOutput(device->name, usage);
        }
        else {
            if (state)
              mm->openInput(device->name, usage);
            else
              mm->closeInput(device->name, usage);
        }
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
            addColumnCheckbox("App Enable", MidiColumnOutput);
            addColumnCheckbox("App Export", MidiColumnExport);
            addColumnCheckbox("App Sync", MidiColumnOutputSync);
            addColumnCheckbox("App Thru", MidiColumnThru);
            addColumnCheckbox("Plugin Enable", MidiColumnPluginOutput);
            addColumnCheckbox("Plugin Export", MidiColumnPluginExport);
            addColumnCheckbox("Plugin Sync", MidiColumnPluginOutputSync);
            addColumnCheckbox("Plugin Thru", MidiColumnPluginThru);
        }
        else {
            addColumn("Name", MidiColumnName);
            addColumnCheckbox("App Enable", MidiColumnInput);
            addColumnCheckbox("App Sync", MidiColumnInputSync);
            addColumnCheckbox("Plugin Enable", MidiColumnPluginInput);
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
            loadDevices(config->midiExport, MidiColumnExport);
            loadDevices(config->midiOutputSync, MidiColumnOutputSync);
            loadDevices(config->midiThru, MidiColumnThru);
            loadDevices(config->pluginMidiOutput, MidiColumnPluginOutput);
            loadDevices(config->pluginMidiExport, MidiColumnPluginExport);
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

/**
 * Convert the table model back into the MachineConfig
 */
void MidiDeviceTable::save(MachineConfig* config)
{
    if (isOutput) {
        config->midiOutput = getDevices(MidiColumnOutput);
        config->midiExport = getDevices(MidiColumnExport);
        config->midiOutputSync = getDevices(MidiColumnOutputSync);
        config->midiThru = getDevices(MidiColumnThru);
        config->pluginMidiOutput = getDevices(MidiColumnPluginOutput);
        config->pluginMidiExport = getDevices(MidiColumnPluginExport);
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

/**
 * Build a csv of all devices with a given column check.
 */
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
    // this was a non-interactive model change so have to refresh
    updateContent();
}

void MidiDeviceTable::forceCheck(MidiDeviceColumn colid, int selectedRow)
{
    MidiDeviceTableRow* row = devices[selectedRow];
    if (!row->checks.contains(colid))
      row->checks.add(colid);

    // this was a non-interactive model change so have to refresh
    updateContent();
}

void MidiDeviceTable::forceUncheck(MidiDeviceColumn colid, int selectedRow)
{
    MidiDeviceTableRow* row = devices[selectedRow];
    row->checks.removeAllInstancesOf(colid);

    // this was a non-interactive model change so have to refresh
    updateContent();
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
