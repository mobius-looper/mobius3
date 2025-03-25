/**
 * Shim between BindingEditor and two different implementations,
 * the old tab based BindingTargetSelector and the new TreeTargetSelector.
 * Won't need this once the tree selector works properly, but might be
 * nice to keep for the future.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "OldBindingTargetSelector.h"
#include "TreeTargetSelector.h"

class TargetSelectorWrapper : public juce::Component, OldBindingTargetSelector::Listener
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void bindingTargetClicked() = 0;
    };

    TargetSelectorWrapper(class Supervisor* s);
    ~TargetSelectorWrapper();
    
    void setListener(Listener* l);
    void load();
    void reset();
    void select(class Binding* b);
    void capture(class Binding* b);
    bool isTargetSelected();

    void resized() override;

    void bindingTargetClicked(OldBindingTargetSelector* bts) override;
  
  private:

    Listener* listener = nullptr;

    bool useNew = false;
    
    OldBindingTargetSelector oldSelector;
    TreeTargetSelector newSelector;
};
