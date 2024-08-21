/**
 * Status element to display alert messages from the engine.
 */

#pragma once

#include <JuceHeader.h>

#include "../../Supervisor.h"
#include "StatusElement.h"

class AlertElement : public StatusElement, public Supervisor::AlertListener
{
  public:
    
    AlertElement(class StatusArea* a);
    ~AlertElement();
    void configure() override;

    void alertReceived(juce::String msg) override;
    
    void update(class MobiusState* state) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    juce::String alert;
    int timeout = 0;
    int alertDuration = 0;
    
};

    
