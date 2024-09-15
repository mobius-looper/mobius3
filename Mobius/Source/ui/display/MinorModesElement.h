/**
 * Status element to display minor modes
 */

#pragma once

#include <JuceHeader.h>

#include "StatusElement.h"

class MinorModesElement : public StatusElement
{
  public:
    
    MinorModesElement(class StatusArea* area);
    ~MinorModesElement();

    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    
};

    
