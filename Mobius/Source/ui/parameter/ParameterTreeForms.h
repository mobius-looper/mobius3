/**
 * A combination of a tree and a form collection to visualize one ValueSet.
 */

#pragma once

#include <JuceHeader.h>

#include "SymbolTree.h"
#include "../session/SessionEditorTree.h"
#include "../session/SessionFormCollection.h"

class ParameterTreeForms : public juce::Component, public SymbolTree::Listener
{
  public:
    
    SessionTreeForms();
    ~SessionTreeForms();

    void initialize(class Provider* p, juce::String treeName);
    void decache();
    
    void load(class ValueSet* src);
    void save(class ValueSet* dest);
    void cancel();
    
    void resized() override;

    void symbolTreeClicked(class SymbolTreeItem* item) override;
    
  private:

    class Provider* provider = nullptr;
    juce::String treeName;
    SessionEditorTree tree;
    SessionFormCollection forms;

    juce::StretchableLayoutManager verticalLayout;
    std::unique_ptr<juce::StretchableLayoutResizerBar> verticalDividerBar;
    
};
