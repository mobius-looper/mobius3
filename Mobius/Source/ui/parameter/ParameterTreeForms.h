/**
 * A combination of a tree and a form collection to visualize one ValueSet.
 */

#pragma once

#include <JuceHeader.h>

#include "../session/SymbolTree.h"
#include "ParameterEditorTree.h"
#include "ParameterFormCollection.h"

class ParameterTreeForms : public juce::Component, public SymbolTree::Listener
{
  public:
    
    ParameterTreeForms();
    ~ParameterTreeForms();

    void initialize(class Provider* p, class ValueSet* set);
    void decache();
    
    void save(class ValueSet* dest);
    void cancel();
    
    void resized() override;

    void symbolTreeClicked(class SymbolTreeItem* item) override;
    
  private:

    class Provider* provider = nullptr;
    class ValueSet* valueSet = nullptr;

    ParameterEditorTree tree;
    ParameterFormCollection forms;

    juce::StretchableLayoutManager verticalLayout;
    std::unique_ptr<juce::StretchableLayoutResizerBar> verticalDividerBar;
    
    class ParameterForm* buildForm(class SymbolTreeItem* parent);
};
