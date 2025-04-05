/**
 * ConfigEditor for editing Bindings.
 */

#pragma once

#include <JuceHeader.h>

#include "../config/ConfigEditor.h"
#include "../script/TypicalTable.h"
#include "../parameter/SymbolTree.h"

#include "BindingSetTable.h"
#include "BindingDetails.h"

class BindingEditor : public ConfigEditor,
                      public TypicalTable::Listener,
                      public BindingDetailsListener,
                      public SymbolTree::Listener,
                      public juce::DragAndDropContainer
{
  public:
    
    BindingEditor(class Supervisor* s);
    virtual ~BindingEditor();

    juce::String getTitle() override {return "Bindings";}
    class Provider* getProvider();

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    void decacheForms() override;
    
    void resized() override;

    void typicalTableChanged(class TypicalTable* t, int row) override;

    void show(int index);

    bool checkName(class BindingSet* src, juce::String newName, juce::StringArray& errors);
    void bindingSetTableNew(class BindingSet* neu, juce::StringArray& errors);
    void bindingSetTableCopy(juce::String newName, juce::StringArray& errors);
    void bindingSetTableRename(juce::String newName, juce::StringArray& errors);
    void bindingSetTableDelete(juce::StringArray& errors);

    void showBinding(class Binding* b);
    void bindingSaved();
    void bindingCanceled();
    void moveBinding(int sourceRow, int dropRow);
    
    void symbolTreeClicked(class SymbolTreeItem* item);
    void symbolTreeDoubleClicked(class SymbolTreeItem* item);
    
  protected:

    int currentSet = -1;
    bool capturing = false;
    
    std::unique_ptr<class BindingSets> bindingSets;
    std::unique_ptr<class BindingSets> revertSets;
    
    std::unique_ptr<class BindingSetTable> setTable;
    std::unique_ptr<class BindingTree> bindingTree;
    juce::OwnedArray<class BindingSetContent> contents;

    BindingDetailsPanel bindingDetails;
    
    void install(class BindingSets* sets, bool buttons);
    class BindingSet* getSourceBindingSet(juce::String action, juce::StringArray& errors);
    void addNew(class BindingSet* set);
    
};
