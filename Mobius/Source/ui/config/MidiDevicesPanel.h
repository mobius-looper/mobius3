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

class MidiDeviceTable : public BasicTable
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
    int getNumRows();
    juce::String getCellText(int row, int col);
    bool getCellCheck(int row, int column);
    void setCellCheck(int row, int column, bool state);
    
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

class MidiDevicesPanel : public ConfigPanel, public Field::Listener, public MidiManager::Listener
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

    // Field listener
    void fieldChanged(Field* field) override;

    // MidiManager::Listener
    void midiMessage(const juce::MidiMessage& message, juce::String& source) override;
    
  private:

    void render();
    void initForm();

    MidiDevicesContent mdcontent;
    Form form;
    LogPanel log;
    
    class Field* inputField = nullptr;
    class Field* outputField = nullptr;
    class Field* pluginOutputField = nullptr;

    BasicTabs tabs;
    MidiDeviceTable inputTable;
    MidiDeviceTable outputTable;
    
};

