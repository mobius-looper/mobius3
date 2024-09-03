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
#include "ScriptLibraryTable.h"

class ScriptConfigEditor : public ConfigEditor
{
  public:
    
    ScriptConfigEditor(class Supervisor* s);
    ~ScriptConfigEditor();

    juce::String getTitle() override {return "Scripts";}
    
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

  private:

    BasicTabs tabs;
    ScriptLibraryTable library;
    ScriptTable externals;

    
};
