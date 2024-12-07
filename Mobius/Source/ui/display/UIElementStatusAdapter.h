/**
 * Temporary adapter betwen the new UIElement interface and the old StatusElement interface.
 */

#pragma once

#include "StatusElement.h"

class UIElementStatusWrapper : public StatusElement
{
  public:

    UIElementStatusWrapper(class StatusArea* a, UIElement* el);
    ~UIElementStatusWrapper();

    void configure() override;
    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    UIElement* element = nullptr;

};
