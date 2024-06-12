/**
 * ConfigPanel to configure MIDI devices when running standalone.
 *  
 * This one is more complicated than most ConfigPanels with the
 * MIDI event logging window under the device selection form.
 * Kind of not liking the old decision to have ConfigPanel with
 * a single content component, or at least be able to give it a
 * content component to use?
 */

#pragma once

#include <JuceHeader.h>

#include "../../MidiManager.h"

#include "../../test/BasicTabs.h"
#include "../common/Form.h"
#include "../common/Field.h"
#include "../common/BasicTable.h"

#include "LogPanel.h"
#include "ConfigPanel.h"

class MidiDeviceTableRow
{
  public:

    MidiDeviceTableRow() {}
    ~MidiDeviceTableRow() {}

    juce::String name;
    // true if this was in DeviceConfig but not found
    bool missing = false;

    bool appControl = false;
    bool appSync = false;
    bool pluginControl = false;
    bool pluginSync = false;
};

class MidiDeviceTable : public BasicTable, public BasicTable::Model
{
  public:

    MidiDeviceTable();
    ~MidiDeviceTable() {}

    void setOuput(bool b) {
        isOutput = b;
    }

    void init(bool output);
    void load(class DeviceConfig* config);
    void save(class DeviceConfig* config);

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
    void loadDevices(juce::String names, bool sync, bool plugin);
    juce::String getDevices(bool sync, bool plugin);

    
};
    
class MidiDevicesContent : public juce::Component
{
  public:
    MidiDevicesContent() {setName("MidiDevicesContent"); };
    ~MidiDevicesContent() {};
    void resized() override;
};

class MidiDevicesPanel : public ConfigPanel, public MidiManager::Listener, public juce::Timer
{
  public:
    MidiDevicesPanel(class ConfigEditor*);
    ~MidiDevicesPanel();

    // ConfigPanel overloads
    void showing() override;
    void hiding() override;
    void load() override;
    void save() override;
    void cancel() override;

    // MidiManager::Listener
    void midiMessage(const juce::MidiMessage& message, juce::String& source) override;
    void timerCallback() override;
    
  private:

    MidiDevicesContent mdcontent;
    LogPanel log;
    BasicTabs tabs;
    MidiDeviceTable inputTable;
    MidiDeviceTable outputTable;

    juce::MidiMessage pluginMessage;
    bool pluginMessageQueued = false;
    
};

