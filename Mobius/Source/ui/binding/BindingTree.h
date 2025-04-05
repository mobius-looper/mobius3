
#pragma once

#include <JuceHeader.h>

#include "../parameter/SymbolTree.h"

class BindingTree : public SymbolTree
{
  public:

    constexpr static const char* DragPrefix = "BindingTree:";
    
    BindingTree();
    ~BindingTree();

    void setDraggable(bool b);

    void initialize(class Provider* p);
    
    void selectFirst();

  private:

    bool draggable = false;
    
    void addFunctions(Provider* p);
    void addParameters(Provider* p);
    void addStructures(Provider* p);
    
    bool isFiltered(class Symbol* s, class ParameterProperties* props);
    void hideEmptyCategories(class SymbolTreeItem* node);

};

