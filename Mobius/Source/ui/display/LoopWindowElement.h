/**
 * Status element to display the moving loop window.
 * This was never used much, and I'm sure we could do a lot better
 * now with the visuals.
 */

#pragma once

#include <JuceHeader.h>

#include "StatusElement.h"

class LoopWindowElement : public StatusElement
{
  public:
    
    LoopWindowElement(class StatusArea* area);
    ~LoopWindowElement();

    void update(class MobiusState* state) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    int mWindowOffset = -1;
    int mWindowFrames = 0;
    int mHistoryFrames = 0;
    
};

    
