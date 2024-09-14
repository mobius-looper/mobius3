/**
 * Status element to display the current loop's mode.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Query.h"

#include "StatusElement.h"

class LoopMeterElement : public StatusElement
{
  public:
    
    LoopMeterElement(class StatusArea* area);
    ~LoopMeterElement();

    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    int lastFrames = 0;
    int lastFrame = 0;
    int lastSubcycles = 0;
    
    int getMeterOffset(int frame, int frames);

};

    
