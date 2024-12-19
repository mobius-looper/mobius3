/**
 * ConfigEditor for editing the script file registry.
 */

#pragma once

#include <JuceHeader.h>

#include "../../script/ScriptClerk.h"
#include "../common/BasicTabs.h"

#include "ConfigEditor.h"
#include "ScriptExternalTable.h"
#include "ScriptLibraryTable.h"
#include "ScriptSymbolTable.h"

class ScriptConfigEditor : public ConfigEditor, public ScriptClerk::Listener
{
  public:
    
    ScriptConfigEditor(class Supervisor* s);
    ~ScriptConfigEditor();

    juce::String getTitle() override {return "Script Library";}
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

    // ScriptExternalTable callback
    void scriptExternalTableChanged();
    
  private:

    BasicTabs tabs;
    ScriptSymbolTable symbols;
    ScriptLibraryTable library;
    ScriptExternalTable externals;

    
};
