/**
 * ConfigEditor for editing ParameterSets.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../config/ConfigEditor.h"
#include "../script/TypicalTable.h"
#include "ParameterSetTable.h"
#include "../session/DynamicTreeForms.h"

class ParameterEditor : public ConfigEditor,
                        public TypicalTable::Listener
                        
{
  public:
    
    ParameterEditor(class Supervisor* s);
    ~ParameterEditor();

    juce::String getTitle() override {return "Parameter Sets";}

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    void decacheForms() override;
    
    void resized() override;

    class Provider* getProvider();

    void typicalTableChanged(class TypicalTable* t, int row) override;

    void show(int index);
    
  private:

    int currentSet = -1;

    std::unique_ptr<class ParameterSets> parameters;
    std::unique_ptr<class ParameterSets> revertParameters;
    std::unique_ptr<class ParameterSetTable> table;

    juce::OwnedArray<class DynamicTreeForms> treeForms;
};
