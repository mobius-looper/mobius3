/**
 * ConfigEditor for editing the Presets.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Preset.h"
#include "../common/Form.h"

#include "ConfigEditor.h"

class PresetEditor : public ConfigEditor
{
  public:
    
    PresetEditor(class Supervisor* s);
    ~PresetEditor();

    juce::String getTitle() override {return "Presets";}

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    
    void objectSelectorSelect(int ordinal) override;
    void objectSelectorRename(juce::String newName) override;
    void objectSelectorDelete() override;
    void objectSelectorNew(juce::String name) override;

    void resized() override;

  private:

    void refreshObjectSelector();
    void initForm();
    void addField(const char* tab, class UIParameter* p, int col = 0);

    void loadPreset(int index);
    void savePreset(int index);
    Preset* getSelectedPreset();
    
    juce::OwnedArray<Preset> presets;
    // another copy for revert
    juce::OwnedArray<Preset> revertPresets;
    int selectedPreset = 0;

    Form form;

};
