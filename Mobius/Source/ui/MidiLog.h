
#pragma once

#include <JuceHeader.h>
#include "common/LogPanel.h"

class MidiLog : public LogPanel
{
  public:

    MidiLog() {}
    ~MidiLog() {}

    void midiMessage(const juce::MidiMessage& message, juce::String& source);
    void showOpen();

  private:

};
