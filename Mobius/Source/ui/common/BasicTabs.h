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

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void basicTabsChanged(int oldIndex, int newIndex) = 0;
    };
    void setListener(Listener* l);

    void add(juce::String name, juce::Component* content);
    void show(int index);
    void currentTabChanged(int newIndex, const juce::String& newName) override;

  private:

    Listener* listener = nullptr;
    int tabIndex = 0;
    
};

