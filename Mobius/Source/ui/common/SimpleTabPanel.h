/*
 * Provides a basic tabbed component.
 *
 */

#pragma once

#include <JuceHeader.h>

class SimpleTabPanel : public juce::Component
{
  public:

    SimpleTabPanel();
    ~SimpleTabPanel();

    void addTab(juce::String name, juce::Component* content);
    void addTab(juce::String name);
    void showTab(int index);
    
    void setBackgroundColor(juce::Colour color);
    
    void resized();
    
  protected:

    juce::TabbedComponent tabs {juce::TabbedButtonBar::Orientation::TabsAtTop};

};    
