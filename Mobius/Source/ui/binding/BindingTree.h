
#pragma once

#include <JuceHeader.h>

#include "../parameter/SymbolTree.h"

class BindingTree : public SymbolTree
{
  public:

    class Listener {
      public:
        virtual ~Listener() {}
        virtual void bindingTreeClicked(class Symbol* s);
    };
    
    BindingTree();
    ~BindingTree();

    void setListener(Listener* l) {
        listener = l;
    }
    
    void initialize(class Provider* p);
    
    void selectFirst();

  private:

    Listener* listener = nullptr;

    void addFunctions(Provider* p);
    void addParameters(Provider* p);
    void addStructures(Provider* p);
    
    bool isFiltered(class Symbol* s, class ParameterProperties* props);
    void hideEmptyCategories(class SymbolTreeItem* node);

};

