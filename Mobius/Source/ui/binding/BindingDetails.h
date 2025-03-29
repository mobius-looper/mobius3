
#pragma once

#include <JuceHeader.h>
#include "../BasePanel.h"
#include "BindingTree.h"
#include "BindingForms.h"

class BindingContent : public juce::Component
{
  public:

    static const int FontHeight = 20;
    static const int TextHeight = 100;

    BindingContent();
    ~BindingContent() {}

    void initialize(class Provider* p);
    void load(class Binding* b);
    
    void resized() override;

  private:

    class Provider* provider = nullptr;
    BindingTree tree;
    BindingForms forms;
};    

class BindingDetailsPanel : public BasePanel
{
  public:

    BindingDetailsPanel();
    ~BindingDetailsPanel() {}

    void initialize();
    void initialize(class Provider* p);

    void show(juce::Component* parent, class Binding* b);
    void close() override;
    
    
  private:

    BindingContent content;
};

