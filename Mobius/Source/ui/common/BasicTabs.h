/**
 * Slight extention to TabbedComponent that simplifies some things.
 * An eventual replacement for common/SimpleTabPanel.
 */

#pragma once

class BasicTabs : public juce::TabbedComponent
{
  public:

    BasicTabs();
    ~BasicTabs();

    void add(juce::String name, juce::Component* content);
    void show(int index);
    void currentTabChanged(int newIndex, const juce::String& newName) override;
    
};

