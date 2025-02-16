/**
 * Display a tree of Session parameter using TreeNode and TreeForm
 * to define the structure of the tree and the forms to display when
 * each node is clicked.
 */

#pragma once

#include <JuceHeader.h>

#include "SymbolTree.h"

class SessionEditorTree : public SymbolTree
{
  public:

    SessionEditorTree();
    ~SessionEditorTree();

    void load(class Provider* p, juce::String treeName);
    
    class SymbolTreeItem* getFirst();
    void selectFirst();

  private:

    class Provider* provider = nullptr;

    void intern(class SymbolTreeItem* parent, juce::String treepath, class TreeNode* node);
    void addSymbol(class SymbolTreeItem* parent, juce::String name);
    
    
};
