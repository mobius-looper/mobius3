
#pragma once

#include <JuceHeader.h>

#include "../parameter/SymbolTree.h"

class TreeTargetSelector : public juce::Component
{
  public:
    
    TreeTargetSelector(class Supervisor* s);
    ~TreeTargetSelector();

    void load();
    void save();
    void reset();
    void select(class OldBinding* b);
    void capture(class OldBinding* b);
    bool isTargetSelected();

    void resized() override;

  private:

    class Supervisor* supervisor = nullptr;
    SymbolTree tree;
    
};

    
