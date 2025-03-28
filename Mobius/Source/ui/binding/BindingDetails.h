
#pragma once

#include <JuceHeader.h>
#include "../BasePanel.h"

class BindingContent : public juce::Component
{
  public:

    static const int FontHeight = 20;
    static const int TextHeight = 100;

    BindingContent();
    ~BindingContent() {}

    void setMessage(juce::String msg);
    void addMessage(juce::String msg);
    
    void resized() override;

  private:

    juce::Label text;
    // juce::StringArray messages;

};    

class BindingDetailsPanel : public BasePanel
{
  public:

    BindingDetailsPanel();
    ~BindingDetailsPanel() {}

    void initialize();

    void show(juce::Component* parent, juce::String message);
    void close() override;
    
  private:

    BindingContent content;
};

