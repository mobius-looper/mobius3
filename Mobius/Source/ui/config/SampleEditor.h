/**
 * ConfigEditor for editing the sample file registry.
 */

#pragma once

#include <JuceHeader.h>

#include "ConfigEditor.h"
#include "SampleTable.h"

class SampleEditor : public ConfigEditor
{
  public:
    
    SampleEditor(class Supervisor* s);
    ~SampleEditor();

    juce::String getTitle() override {return "Samples";}

    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

  private:

    SampleTable table;

};
