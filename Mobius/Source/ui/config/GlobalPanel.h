/**
 * ConfigPanel to edit track setups
 */

#pragma once

#include <JuceHeader.h>

#include "../common/Form.h"
#include "../../test/BasicForm.h"
#include "../../test/BasicInput.h"
#include "ConfigPanel.h"

class GlobalPanel : public ConfigPanel 
{
  public:
    GlobalPanel(class ConfigEditor*);
    ~GlobalPanel();

    // overloads called by ConfigPanel
    void load();
    void save();
    void cancel();

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
    
};
