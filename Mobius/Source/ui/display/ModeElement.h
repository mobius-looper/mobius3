/**
 * Status element to display the current loop's mode.
 */

#pragma once

#include <JuceHeader.h>

#include "StatusElement.h"

class ModeElement : public StatusElement
{
  public:
    
    ModeElement(class StatusArea* area);
    ~ModeElement();

    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

};

    
