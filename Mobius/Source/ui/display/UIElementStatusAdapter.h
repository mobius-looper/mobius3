/**
 * Temporary adapter betwen the new UIElement interface and the old StatusElement interface.
 */

#pragma once

#include "StatusElement.h"

class UIElementStatusAdapter : public StatusElement
{
  public:

    UIElementStatusAdapter(class StatusArea* a, class UIElement* el);
    ~UIElementStatusAdapter();

    void configure() override;
    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    std::unique_ptr<class UIElement> element;

};
