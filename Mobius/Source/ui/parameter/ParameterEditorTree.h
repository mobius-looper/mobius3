/**
 * Display a tree of Session parameter using TreeNode and TreeForm
 * to define the structure of the tree and the forms to display when
 * each node is clicked.
 */

#pragma once

#include <JuceHeader.h>

#include "../session/SymbolTree.h"

class ParameterEditorTree : public SymbolTree
{
  public:

    ParameterEditorTree();
    ~ParameterEditorTree();

    void load(class Provider* p, class ValueSet* set);
    
    class SymbolTreeItem* getFirst();
    void selectFirst();

  private:

    class Provider* provider = nullptr;

    //void intern(class SymbolTreeItem* parent, juce::String treepath, class TreeNode* node);
    //void addSymbol(class SymbolTreeItem* parent, juce::String name, juce::String suppressPrefix);
    
    
};
