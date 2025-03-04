/**
 * Display a tree of parameters dynamically generated using either
 * the symbol table or an existing ValueSet.
 */

#pragma once

#include <JuceHeader.h>

#include "../session/SymbolTree.h"

class DynamicParameterTree : public SymbolTree,
                             public juce::DragAndDropContainer
{
  public:

    DynamicParameterTree();
    ~DynamicParameterTree();

    // have to call this somethign else since SymbolTree already has a listener
    class DropListener {
      public:
        virtual ~DropListener() {}
        virtual void dynamicParameterTreeDrop(DynamicParameterTree* dpt, juce::String desc) = 0;
    };

    void setDropListener(DropListener* l);

    // initialize from the symbol table
    void initialize(class Provider* p);

    // initialize from a value set
    void initialize(class Provider* p, class ValueSet* set);
    
    class SymbolTreeItem* getFirst();
    void selectFirst();

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails&) override;

  private:

    DropListener* dropListener = nullptr;

    void internCategories();
    
};
