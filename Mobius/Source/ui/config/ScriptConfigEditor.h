/**
 * ConfigEditor for editing the script file registry.
 *
 * todo: ScriptTable and SampleTable are basically identical.
 * Refactor this to share.
 */

#pragma once

#include <JuceHeader.h>

#include "../../script/ScriptClerk.h"
#include "../common/BasicTabs.h"

#include "ConfigEditor.h"
#include "ScriptTable.h"
#include "ScriptLibraryTable.h"

class ScriptConfigEditor : public ConfigEditor, public ScriptClerk::Listener
{
  public:
    
    ScriptConfigEditor(class Supervisor* s);
    ~ScriptConfigEditor();

    juce::String getTitle() override {return "Scripts";}
    bool isImmediate() override {return true;}
    
    void showing() override;
    void hiding() override;
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

    // clerk listener
    void scriptFileSaved(class ScriptRegistry::File* file) override;
    void scriptFileAdded(class ScriptRegistry::File* file) override;
    void scriptFileDeleted(class ScriptRegistry::File* file) override;

    // ScriptTable callback
    void scriptTableChanged();
    
  private:

    BasicTabs tabs;
    ScriptLibraryTable library;
    ScriptTable externals;

    
};
