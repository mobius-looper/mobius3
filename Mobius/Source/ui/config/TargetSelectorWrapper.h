/**
 * Shim between BindingEditor and two different implementations,
 * the old tab based BindingTargetSelector and the new TreeTargetSelector.
 * Won't need this once the tree selector works properly, but might be
 * nice to keep for the future.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "BindingTargetSelector.h"
#include "TreeTargetSelector.h"

class TargetSelectorWrapper : public juce::Component, BindingTargetSelector::Listener
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
    void select(class OldBinding* b);
    void capture(class OldBinding* b);
    bool isTargetSelected();

    void resized() override;

    void bindingTargetClicked(BindingTargetSelector* bts) override;
  
  private:

    Listener* listener = nullptr;

    bool useNew = false;
    
    BindingTargetSelector oldSelector;
    TreeTargetSelector newSelector;
};
