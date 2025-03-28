
#pragma once

#include <JuceHeader.h>
#include "../BasePanel.h"
#include "BindingTree.h"

class BindingContent : public juce::Component
{
  public:

    static const int FontHeight = 20;
    static const int TextHeight = 100;

    BindingContent();
    ~BindingContent() {}

    void initialize(Provider* p);
    
    void resized() override;

  private:

    BindingTree tree;
    
};    

class BindingDetailsPanel : public BasePanel
{
  public:

    BindingDetailsPanel();
    ~BindingDetailsPanel() {}

    void initialize();

    void show(juce::Component* parent);
    void close() override;
    
    void initialize(Provider* p);
    
  private:

    BindingContent content;
};

