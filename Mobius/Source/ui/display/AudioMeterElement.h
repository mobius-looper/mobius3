/**
 * Basic level meter
 */

#pragma once

#include <JuceHeader.h>

#include "StatusElement.h"

class AudioMeterElement : public StatusElement
{
  public:
    
    AudioMeterElement(class StatusArea* area);
    ~AudioMeterElement();

    void update(class MobiusState* state) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    int range = 0;
    int savedValue = 0;
    int savedLevel = 0;

};

    
