/**
 * ConfigEditor for editing the SystemConfig object.
 */

#pragma once

#include <JuceHeader.h>

#include "ConfigEditor.h"
#include "../common/BasicTabs.h"
#include "../common/ValueSetForm.h"

class SystemEditor : public ConfigEditor
{
  public:
    
    SystemEditor(class Supervisor* s);
    ~SystemEditor();

    juce::String getTitle() override {return "System";}

    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

  private:

    std::unique_ptr<ValueSet> values;

    BasicTabs tabs;
    
    ValueSetForm plugin;
    ValueSetForm files;
    
    void initForm(ValueSetForm& form, const char* defname);
    void loadPluginValues();
    void savePluginValues();
    int getPortValue(const char* name, int max);

};
