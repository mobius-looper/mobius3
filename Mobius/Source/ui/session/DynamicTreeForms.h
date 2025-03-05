/**
 * A combination of a tree and a form collection to visualize a ValueSet.
 *
 * These differ from SessionTreeForms in that both the tree and the form collection
 * are built dynamically from the symbol table and/or an existing ValueSet rather
 * than being guided by a TreeNode and TreeForm definition.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../parameter/SymbolTree.h"
#include "DynamicParameterTree.h"
#include "DynamicFormCollection.h"

class DynamicTreeForms : public juce::Component,
                         public SymbolTree::Listener,
                         public ParameterForm::Listener,
                         public DropTreeView::Listener,
                         // it is important that this be as high as possible to encompass
                         // all the drag sources
                         public juce::DragAndDropContainer
{
  public:
    
    DynamicTreeForms();
    ~DynamicTreeForms();

    // initialize from the SymbolTable
    void initialize(class Provider* p);
    
    // initialize from an existing ValueSet
    void initialize(class Provider* p, class ValueSet* set);

    // when initialized from the symbol table, load the values
    void load(class ValueSet* set);
    
    void decache();
    
    void save();
    void save(class ValueSet* set);
    
    void cancel();
    
    void resized() override;

    void symbolTreeClicked(class SymbolTreeItem* item) override;

    void parameterFormDrop(class ParameterForm* src, juce::String desc) override;

    void dropTreeViewDrop(DropTreeView* dtv, const juce::DragAndDropTarget::SourceDetails& details) override;
    


  private:

    class Provider* provider = nullptr;
    class ValueSet* valueSet = nullptr;

    bool restricted = false;
    DynamicParameterTree tree;
    DynamicFormCollection forms;

    juce::StretchableLayoutManager verticalLayout;
    std::unique_ptr<juce::StretchableLayoutResizerBar> verticalDividerBar;
    
    class ParameterForm* buildForm(class SymbolTreeItem* parent);
};
