/**
 * Panel to edit MIDI bindings
 */

#pragma once

#include <JuceHeader.h>

#include "../../MidiManager.h"
#include "../common/SimpleTable.h"
#include "../common/ButtonBar.h"
#include "../common/Field.h"

#include "BindingEditor.h"

class MidiEditor : public BindingEditor, public MidiManager::Monitor
{
  public:
    MidiEditor(class Supervisor* s);
    ~MidiEditor();

    juce::String getTitle() override {return juce::String("MIDI Bindings");}

    void prepare() override;
    void showing() override;
    void hiding() override;

    juce::String renderSubclassTrigger(Binding* b) override;
    bool isRelevant(class Binding* b) override;
    void addSubclassFields() override;
    bool wantsCapture() override {return true;} 
    bool wantsRelease() override {return true;} 
    void refreshSubclassFields(class Binding* b) override;
    void captureSubclassFields(class Binding* b) override;
    void resetSubclassFields() override;

    void midiMonitor(const juce::MidiMessage& message, juce::String& source) override;
    bool midiMonitorExclusive() override;

  private:

    Field* messageType = nullptr;
    Field* messageChannel = nullptr;
    Field* messageValue = nullptr;

    juce::MidiMessage pluginMessage;
    bool pluginMessageQueued = false;

    juce::String renderCapture(const juce::MidiMessage& msg);

};
