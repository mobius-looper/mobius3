/**
 * Status element to display a floating strip of track controls.
 */

#pragma once

#include <JuceHeader.h>

#include "StatusElement.h"
#include "TrackStrip.h"

class FloatingStripElement : public StatusElement
{
  public:
    
    FloatingStripElement(class StatusArea* area);
    ~FloatingStripElement();

    // if we have more than one, give them a name
    juce::String name;
    
    void configure() override;
    void update(class MobiusState* state) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;
    
  private:

    TrackStrip strip {this};
    
};

    
