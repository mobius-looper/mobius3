/**
 * Status element to display the current loop's play position.
 */

#pragma once

#include <JuceHeader.h>

#include "StatusElement.h"

class CounterElement : public StatusElement
{
  public:
    
    CounterElement(class StatusArea* area);
    ~CounterElement();

    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    // numbers currently being displayed

    int loopNumber = 0;
    int loopFrame = 0;
    int cycle = 0;
    int cycles = 0;

    // captured this from config, I guess to keep from having
    // to go back there
    int sampleRate = 0;

    int digitWidth = 0;
};

    
