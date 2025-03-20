/**
 * ConfigEditor for editing Bindings.
 */

#pragma once

#include <JuceHeader.h>

#include "../config/ConfigEditor.h"
#include "../script/TypicalTable.h"

#include "BindingTable.h"

class BindingEditor : public ConfigEditor,
                      public TypicalTable::Listener
{
  public:
    
    BindingEditor(class Supervisor* s);
    ~BindingEditor();

    juce::String getTitle() override {return "Parameter Bindings";}

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    void decacheForms() override;
    
    void resized() override;

    void typicalTableChanged(class TypicalTable* t, int row) override;

    void show(int index);

    void overlayTableNew(juce::String newName, juce::StringArray& errors);
    void overlayTableCopy(juce::String newName, juce::StringArray& errors);
    void overlayTableRename(juce::String newName, juce::StringArray& errors);
    void overlayTableDelete(juce::StringArray& errors);
    
  private:

    int currentSet = -1;

    std::unique_ptr<class ParameterSets> overlays;
    std::unique_ptr<class ParameterSets> revertBindings;
    std::unique_ptr<class BindingTable> table;

    juce::OwnedArray<class BindingTreeForms> treeForms;

    bool checkName(juce::String newName, juce::StringArray& errors);
    class ValueSet* getSourceBinding(juce::String action, juce::StringArray& errors);
    void addNew(class ValueSet* set);
    
};
