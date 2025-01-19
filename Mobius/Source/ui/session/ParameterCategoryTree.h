// OBSOLETE: DELETE

/**
 * Display a tree of Session parameter categories to drive the generation
 * of the session parameter editing forms.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"

#include "SymbolTree.h"

class ParameterCategoryTree : public SymbolTree
{
  public:

    ParameterCategoryTree();
    ~ParameterCategoryTree();

    void load(class SymbolTable* table, juce::String includes);

  private:

    
};
