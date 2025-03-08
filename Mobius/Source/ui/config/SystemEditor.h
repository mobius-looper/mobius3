/**
 * ConfigEditor for editing the SystemConfig object.
 */

#pragma once

#include <JuceHeader.h>

#include "ConfigEditor.h"
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

    std::unique_ptr<ValueSetForm> form;
};
