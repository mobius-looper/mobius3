/**
 * ConfigEditor for editing Bindings.
 */

#pragma once

#include <JuceHeader.h>

#include "../config/ConfigEditor.h"
#include "../script/TypicalTable.h"

#include "BindingSetTable.h"
#include "BindingDetails.h"

class BindingEditor : public ConfigEditor,
                      public TypicalTable::Listener
{
  public:
    
    BindingEditor(class Supervisor* s);
    ~BindingEditor();

    juce::String getTitle() override {return "Bindings";}

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    void decacheForms() override;
    
    void resized() override;

    void typicalTableChanged(class TypicalTable* t, int row) override;

    void show(int index);

    void bindingSetTableNew(juce::String newName, juce::StringArray& errors);
    void bindingSetTableCopy(juce::String newName, juce::StringArray& errors);
    void bindingSetTableRename(juce::String newName, juce::StringArray& errors);
    void bindingSetTableDelete(juce::StringArray& errors);

    void showBinding(class Binding* b);
    
  private:

    int currentSet = -1;

    std::unique_ptr<class BindingSets> bindingSets;
    std::unique_ptr<class BindingSets> revertSets;
    std::unique_ptr<class BindingSetTable> setTable;
    juce::OwnedArray<class BindingSetContent> contents;

    BindingDetailsPanel bindingDetails;
    
    //juce::OwnedArray<class BindingTreeForms> treeForms;

    bool checkName(juce::String newName, juce::StringArray& errors);
    class BindingSet* getSourceBindingSet(juce::String action, juce::StringArray& errors);
    void addNew(class BindingSet* set);
    
};
