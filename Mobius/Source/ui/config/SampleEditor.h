/**
 * ConfigEditor for editing the sample file registry.
 *
 * todo: ScriptTable and SampleTable are basically identical.
 * Refactor this to share.
 */

#pragma once

#include <JuceHeader.h>

#include "NewConfigEditor.h"
#include "SampleTable.h"

class SampleEditor : public NewConfigEditor
{
  public:
    
    SampleEditor();
    ~SampleEditor();

    juce::String getTitle() {return "Samples";}

    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

  private:

    SampleTable table;

};
