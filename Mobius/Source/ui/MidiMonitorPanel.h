
#pragma once

#include "../MidiManager.h"
#include "BasePanel.h"
#include "MidiLog.h"

class MidiMonitorContent : public juce::Component
{
  public:

    MidiMonitorContent();
    ~MidiMonitorContent();

    void resized() override;
    
    void showing();
    
    MidiLog log;
    
  private:

};

class MidiMonitorPanel : public BasePanel, public MidiManager::Monitor
{
  public:

    MidiMonitorPanel();
    ~MidiMonitorPanel();

    void showing() override;
    void hiding() override;
    void footerButton(juce::Button* b) override;

    void midiMonitor(const juce::MidiMessage& message, juce::String& source) override;
    bool midiMonitorExclusive() override;
    
  private:

    MidiMonitorContent content;
    juce::TextButton clearButton {"Clear"};
};

    
