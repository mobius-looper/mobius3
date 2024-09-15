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

    Query subcyclesQuery;

    long lastFrames = 0;
    long lastFrame = 0;
    int lastSubcycles = 0;
    
    // for events just maintain a pointer directly to MobiusState
    // which is known to live between udpate and paint
    // can't easily do difference detection on this but we can
    // for the frame pointer which triggers a full refresh
    class MobiusLoopState* loop = nullptr;

    int getMeterOffset(long frame);

};

    
