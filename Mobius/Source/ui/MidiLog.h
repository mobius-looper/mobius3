
#pragma once

#include <JuceHeader.h>
#include "common/LogPanel.h"

class MidiLog : public LogPanel
{
  public:

    MidiLog(class Supervisor* s) {
        supervisor = s;
    }
    ~MidiLog() {}

    void midiMessage(const juce::MidiMessage& message, juce::String& source);
    void showOpen();

  private:

    class Supervisor* supervisor = nullptr;

};
