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

#include "../common/Form.h"
#include "../common/Field.h"

#include "LogPanel.h"
#include "ConfigPanel.h"

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

};

