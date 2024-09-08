/**
 * ConfigEditor for editing the script file registry.
 *
 * todo: ScriptTable and SampleTable are basically identical.
 * Refactor this to share.
 */

#pragma once

#include <JuceHeader.h>

#include "../../script/ScriptClerk.h"

#include "ConfigEditor.h"
#include "ScriptTable.h"
#include "ScriptLibraryTable.h"

class ScriptConfigEditor : public ConfigEditor, public ScriptClerk::Listener
{
  public:
    
    ScriptConfigEditor(class Supervisor* s);
    ~ScriptConfigEditor();

    juce::String getTitle() override {return "Scripts";}

    void showing() override;
    void hiding() override;
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

    // clerk listener
    void scriptFileAdded(class ScriptRegistry::File* file) override;
    void scriptFileDeleted(class ScriptRegistry::File* file) override;

  private:

    BasicTabs tabs;
    ScriptLibraryTable library;
    ScriptTable externals;

    
};
