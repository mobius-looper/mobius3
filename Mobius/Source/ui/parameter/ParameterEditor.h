/**
 * ConfigEditor for editing ParameterSets.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../config/ConfigEditor.h"
#include "ParameterSetTable.h"

class ParameterEditor : public ConfigEditor
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
    
  private:

    std::unique_ptr<class ParameterSets> parameters;
    std::unique_ptr<class ParameterSets> revertParameters;
    std::unique_ptr<class ParameterSetTable> table;
};
