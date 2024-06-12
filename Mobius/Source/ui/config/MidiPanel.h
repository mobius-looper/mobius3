/**
 * Panel to edit MIDI bindings
 */

#pragma once

#include <JuceHeader.h>

#include "../../MidiManager.h"
#include "../common/SimpleTable.h"
#include "../common/ButtonBar.h"
#include "../common/Field.h"

#include "ConfigPanel.h"
#include "BindingPanel.h"

class MidiPanel : public BindingPanel, public MidiManager::Listener, public juce::Timer
{
  public:
    MidiPanel(class ConfigEditor *);
    ~MidiPanel();

    void showing() override;
    void hiding() override;

    juce::String renderSubclassTrigger(Binding* b) override;
    bool isRelevant(class Binding* b) override;
    void addSubclassFields() override;
    void refreshSubclassFields(class Binding* b) override;
    void captureSubclassFields(class Binding* b) override;
    void resetSubclassFields() override;

    void midiMessage(const class juce::MidiMessage& message, juce::String& source) override;
    void timerCallback() override;

  private:

    Field* messageType = nullptr;
    Field* messageChannel = nullptr;
    Field* messageValue = nullptr;
    Field* capture = nullptr;

    juce::MidiMessage pluginMessage;
    bool pluginMessageQueued = false;

};
