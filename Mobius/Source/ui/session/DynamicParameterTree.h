/**
 * Display a tree of parameters dynamically generated using either
 * the symbol table or an existing ValueSet.
 */

#pragma once

#include <JuceHeader.h>

#include "../session/SymbolTree.h"

class DynamicParameterTree : public SymbolTree
{
  public:

    DynamicParameterTree();
    ~DynamicParameterTree();

    // initialize from the symbol table
    void initialize(class Provider* p);

    // initialize from a value set
    void initialize(class Provider* p, class ValueSet* set);
    
    class SymbolTreeItem* getFirst();
    void selectFirst();

  private:

    class Provider* provider = nullptr;
};
