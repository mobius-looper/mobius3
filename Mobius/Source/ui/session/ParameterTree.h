/**
 * A ParametetTree displays a tree view of parameter symbols arranged
 * in a hierarchy.  It may be initialized in two ways:
 *
 *   Static
 *     The structure of the tree is defined by a TreeNode object read
 *     from static.xml
 *
 *   Dynamic
 *     The structure of the tree is guided by iterating over the symbol table
 *     looking for symbols with certain characteristics, and uses properties
 *     of the symbol to build the hiearchy.
 *
 * Static trees are only used for the representation of the Global parameters.
 * 
 * Dynamic trees are used for parameters that are related to track behavior.
 *
 * Both are normally associated with a ParameterFormCollection that defines the forms to
 * display when tree nodes are selected.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "SymbolTree.h"

class ParameterTree : public SymbolTree
{
  public:

    constexpr static const char* DragPrefix = "ParameterTree:";

    ParameterTree();
    ~ParameterTree();

    void initializeStatic(class Provider* p, juce::String treeName);
    void initializeDynamic(class Provider* p);

    class SymbolTreeItem* getFirst();
    void selectFirst();

  private:

    class Provider* provider = nullptr;
    bool draggable = false;

    // static building
    void intern(class SymbolTreeItem* parent, juce::String treepath, class TreeNode* node);
    void addSymbol(class SymbolTreeItem* parent, juce::String name, juce::String suppressPrefix);

    // dynamic building
    void internCategories();
    void initializeSparse(class Provider* p, class ValueSet* set);
    
};
