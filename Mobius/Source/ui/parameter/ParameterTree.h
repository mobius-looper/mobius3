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
 * As a SymbolTree, a Listener provides a callback when nodes are clicked
 * and a DropTreeListener provides a callback for when something is dropped on the tree.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Symbol.h"

#include "SymbolTree.h"

class ParameterTreeComparator
{
  public:

    ParameterTreeComparator(class TreeForm* tf);

    int compareElements(juce::TreeViewItem* first, juce::TreeViewItem* second);

  private:

    class TreeForm* form = nullptr;
};

class ParameterTree : public SymbolTree
{
  public:

    constexpr static const char* DragPrefix = "ParameterTree:";

    ParameterTree();
    ~ParameterTree();

    void initializeStatic(class Provider* p, juce::String treeName);
    void initializeDynamic(class Provider* p);
    void setDraggable(bool b);
    void setFilterNoDefault(bool b);
    void setFilterNoOverlay(bool b);
    void setTrackType(SymbolTrackType t);

    void selectFirst();

  private:

    bool draggable = false;
    bool filterNoDefault = false;
    bool filterNoOverlay = false;
    SymbolTrackType trackType = TrackTypeNone;
    
    // static building
    void intern(class Provider* p, class StaticConfig* scon, class SymbolTreeItem* parent,
                juce::String treepath, class TreeNode* node);
    void addSymbol(class Provider* p, class SymbolTreeItem* parent,
                   juce::String name, juce::String suppressPrefix);

    // dynamic building
    void internCategories();
    void hideEmptyCategories();
    void initializeSparse(class Provider* p, class ValueSet* set);
    bool isFiltered(class Symbol* s, class ParameterProperties* props);
    
};
