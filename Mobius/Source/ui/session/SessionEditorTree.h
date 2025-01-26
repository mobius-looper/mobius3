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

    void intern(class SymbolTreeItem* parent, class TreeNode* node);
    
    
};
