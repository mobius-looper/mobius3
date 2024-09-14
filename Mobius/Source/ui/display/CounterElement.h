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

    // cached width for paint
    int digitWidth = 0;

    // repaint difference detection
    int lastFrame = 0;
    int lastCycle = 0;
    int lastCycles = 0;

};

    
