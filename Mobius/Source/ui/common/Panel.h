/**
 * A basic container component with automatic layout options.
 */

#pragma once

#include <JuceHeader.h>

class Panel : public juce::Component
{
  public:

    enum Orientation {
        Vertical,
        Horizontal
    };

    Panel();
    Panel(Orientation);
    ~Panel() override;

    void setOrientation(Orientation);
    void addOwned(juce::Component* c);
    void addShared(juce::Component* c);
    
    int getPreferredWidth();
    int getPreferredHeight();
    void autoSize();

    // Component overloads
    void resized() override;

  private:

    void init();

    Orientation orientation = Vertical;
    
    juce::OwnedArray<Component> ownedChildren;

};


