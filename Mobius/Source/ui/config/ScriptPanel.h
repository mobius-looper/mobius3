/**
 * ConfigPanel to edit scripts
 */

#pragma once

#include <JuceHeader.h>

#include "ScriptTable.h"
#include "NewConfigPanel.h"
 
class ScriptEditor : public ConfigPanelContent
{
  public:
    
    ScriptEditor();
    ~ScriptEditor();

    // overloads called by ConfigPanel
    void load() override;
    void save() override;
    void cancel() override;

    void resized() override;

  private:

    ScriptTable table;
    juce::String lastFolder;

};

class ScriptPanel : public NewConfigPanel
{
  public:
    ScriptPanel() {
        setName("ScriptPanel");
        setTitle("Scripts");
        setConfigContent(&editor);
    }
    ~ScriptPanel() {
    }

  private:
    ScriptEditor editor;
};
