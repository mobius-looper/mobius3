/**
 * ConfigEditor to configure MIDI devices when running standalone.
 */

#pragma once

#include <JuceHeader.h>

#include "../../MidiManager.h"

#include "../common/BasicTabs.h"
#include "../common/Form.h"
#include "../common/Field.h"
#include "../common/BasicTable.h"
#include "../MidiLog.h"

#include "ConfigEditor.h"

/**
 * Enumeration of column ids for the two tables.  Though one
 * table won't have all of these, it's nice to have a single number
 * space to refer to both of them.
 */
enum MidiDeviceColumn {
    MidiColumnName = 1,
    MidiColumnInput,
    MidiColumnInputSync,
    MidiColumnOutput,
    MidiColumnOutputSync,
    MidiColumnThru,
    MidiColumnPluginInput,
    MidiColumnPluginInputSync,
    MidiColumnPluginOutput,
    MidiColumnPluginOutputSync,
    MidiColumnPluginThru
};

class MidiDeviceTableRow
{
  public:

    MidiDeviceTableRow() {}
    ~MidiDeviceTableRow() {}

    juce::String name;
    // true if this was in DeviceConfig but not found
    bool missing = false;

    juce::Array<MidiDeviceColumn> checks;
};

class MidiDeviceTable : public BasicTable, public BasicTable::Model
{
  public:

    MidiDeviceTable();
    ~MidiDeviceTable() {}

    void setOuput(bool b) {
        isOutput = b;
    }

    void init(class MidiManager* mm, bool output);
    void load(class MachineConfig* config);
    void save(class MachineConfig* config);
    void uncheckOthers(MidiDeviceColumn colid, int selectedRow);
    
    MidiDeviceTableRow* getRow(int row) {
        return devices[row];;
    }
    
    juce::String getName(int row);

    // BasicTable::Model
    int getNumRows() override;
    juce::String getCellText(int row, int col) override;
    juce::Colour getCellColor(int row, int col) override;
    bool getCellCheck(int row, int column) override;
    void setCellCheck(int row, int column, bool state) override;
    
  private:

    bool initialized = false;
    bool isOutput = false;
    juce::OwnedArray<MidiDeviceTableRow> devices;

    MidiDeviceTableRow* getRow(juce::String name);
    void loadDevices(juce::String names, MidiDeviceColumn colid);
    juce::String getDevices(MidiDeviceColumn colid);

    
};
    
class MidiDeviceEditor : public ConfigEditor,
                          public MidiManager::Monitor,
                          public BasicTable::CheckboxListener
{
  public:
    MidiDeviceEditor();
    ~MidiDeviceEditor();

    juce::String getTitle() override {return juce::String("MIDI Devices");}
    
    void showing() override;
    void hiding() override;
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

    // MidiManager::Monitor
    void midiMonitor(const juce::MidiMessage& message, juce::String& source) override;
    bool midiMonitorExclusive() override;
    void midiMonitorMessage(juce::String msg) override;
    
    // BasicTable::CheckboxListener
    void tableCheckboxTouched(BasicTable* table, int row, int col, bool state) override;
    
  private:

    MidiLog log;
    BasicTabs tabs;
    MidiDeviceTable inputTable;
    MidiDeviceTable outputTable;

    // the MachineConfig being edited
    // when in "live" mode this will be the active model
    // extracted from Supervisor::getDeviceConfig
    class MachineConfig* machine = nullptr;
    
    juce::MidiMessage pluginMessage;
    bool pluginMessageQueued = false;
    
};

