/**
 * Panel to edit MIDI bindings
 */

#pragma once

#include <JuceHeader.h>

#include "../../MidiManager.h"
#include "../common/SimpleTable.h"
#include "../common/ButtonBar.h"
//#include "../common/Field.h"
#include "../common/YanField.h"

#include "OldBindingEditor.h"

class MidiEditor : public OldBindingEditor, public MidiManager::Monitor
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
    bool wantsPassthrough() override {return true;} 
    void refreshSubclassFields(class Binding* b) override;
    void captureSubclassFields(class Binding* b) override;
    void resetSubclassFields() override;

    void midiMonitor(const juce::MidiMessage& message, juce::String& source) override;
    bool midiMonitorExclusive() override;

  private:

    bool started = false;
    YanCombo messageType {"Type"};
    YanCombo messageChannel {"Channel"};
    YanInput messageValue {"Value", 10};
    
    juce::MidiMessage pluginMessage;
    bool pluginMessageQueued = false;

    juce::String renderCapture(const juce::MidiMessage& msg);

};
