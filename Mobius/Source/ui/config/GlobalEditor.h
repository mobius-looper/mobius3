/**
 * ConfigEditor to edit global parameters
 */

#pragma once

#include <JuceHeader.h>

#include "../common/Form.h"
#include "../common/BasicForm.h"
#include "../common/BasicInput.h"

#include "ConfigEditor.h"

class GlobalEditor : public ConfigEditor
{
  public:
    GlobalEditor(class Supervisor* s);
    ~GlobalEditor();
    
    juce::String getTitle() override {return "Global Parameters";}

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    
    void resized() override;

  private:

    void render();
    void initForm();
    void addField(const char* tab, class UIParameter* p);
    
    void loadGlobal(class MobiusConfig* c);
    void saveGlobal(class MobiusConfig* c);
    int getPortValue(BasicInput* field, int max);
    
    Form form;
    BasicForm properties;
    BasicInput asioInputs {"Standalone Inputs", 20};
    BasicInput asioOutputs {"Standalone Outputs", 20};
    BasicInput pluginInputs {"Plugin Inputs", 20};
    BasicInput pluginOutputs {"Plugin Outputs", 20};

    Field* ccThreshold = nullptr;
    
};
