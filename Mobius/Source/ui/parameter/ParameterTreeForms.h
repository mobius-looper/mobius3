/**
 * Base class of a component that combinds a ParameterTree with
 * a ParameterFormCollection.  Mostly just provides provides a wrapper
 * component with a slider bar between them.
 */

#pragma once

#include <JuceHeader.h>

#include "ParameterTree.h"
#include "ParameterFormCollection.h"

class ParameterTreeForms : public juce::Component, public SymbolTree::Listener
{
  public:
    
    ParameterTreeForms();
    ~ParameterTreeForms();

    void symbolTreeClicked(class SymbolTreeItem* item);
    
    void resized() override;
    
  protected:

    ParameterTree tree;
    ParameterFormCollection forms;
    juce::String treeName;
    
    juce::StretchableLayoutManager verticalLayout;
    std::unique_ptr<juce::StretchableLayoutResizerBar> verticalDividerBar;
    
};
