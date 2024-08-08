
#pragma once

#include "../MidiManager.h"
#include "BasePanel.h"
#include "MidiLog.h"

class MidiMonitorContent : public juce::Component
{
  public:

    MidiMonitorContent(class Supervisor* s);
    ~MidiMonitorContent();

    void resized() override;
    
    void showing();
    
    MidiLog log;
    
  private:
    class Supervisor* supervisor = nullptr;

};

class MidiMonitorPanel : public BasePanel, public MidiManager::Monitor
{
  public:

    MidiMonitorPanel(class Supervisor* s);
    ~MidiMonitorPanel();

    void showing() override;
    void hiding() override;
    void footerButton(juce::Button* b) override;

    void midiMonitor(const juce::MidiMessage& message, juce::String& source) override;
    bool midiMonitorExclusive() override;
    
  private:
    class Supervisor* supervisor = nullptr;
    MidiMonitorContent content;
    juce::TextButton clearButton {"Clear"};
};

    
