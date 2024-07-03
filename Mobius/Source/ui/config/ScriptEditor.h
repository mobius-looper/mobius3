/**
 * ConfigEditor for editing the script file registry.
 *
 * todo: ScriptTable and SampleTable are basically identical.
 * Refactor this to share.
 */

#pragma once

#include <JuceHeader.h>

#include "ConfigEditor.h"
#include "ScriptTable.h"

class ScriptEditor : public ConfigEditor
{
  public:
    
    ScriptEditor();
    ~ScriptEditor();

    juce::String getTitle() override {return "Scripts";}
    
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

  private:

    ScriptTable table;

};
