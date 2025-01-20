/**
 * A combination of a tree and a form collection.
 * As nodes are selected in a tree, a form from the collection
 * is shown.
 */

#pragma once

#include <JuceHeader.h>

#include "SymbolTree.h"
#include "SessionEditorTree.h"
#include "SessionFormCollection.h"

class SessionTreeForms : public juce::Component, public SymbolTree::Listener
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
    
};
