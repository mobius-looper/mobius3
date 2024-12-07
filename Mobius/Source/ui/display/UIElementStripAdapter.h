/**
 * Temporary adapter betwen the new UIElement interface and the old StripElement interface.
 */

#pragma once

#include "StripElement.h"

class UIElementStripAdapter : public StripElement
{
  public:

    UIElementStripAdapter(class TrackStrip* ts, class UIElement* el);
    ~UIElementStripAdapter();

    void configure() override;
    void update(class MobiusView* view) override;
    int getPreferredWidth() override;
    int getPreferredHeight() override;
    
    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    std::unique_ptr<class UIElement> element;

};
