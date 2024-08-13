/**
 * ConfigEditor for editing the Presets.
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "../common/BasicForm.h"
#include "../common/BasicInput.h"
#include "MultiSelectDrag.h"

#include "ConfigEditor.h"

class GroupEditor : public ConfigEditor
{
  public:
    
    GroupEditor(class Supervisor* s);
    ~GroupEditor();

    juce::String getTitle() override {return "Groups";}

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

    juce::String getInternalName(int ordinal);
    void refreshObjectSelector();
    void initForm();

    void loadGroup(int index);
    void saveGroup(int index);
    class GroupDefinition* getSelectedGroup();
    
    juce::OwnedArray<class GroupDefinition> groups;
    // another copy for revert
    juce::OwnedArray<class GroupDefinition> revertGroups;
    int selectedGroup = 0;

    BasicForm form;
    BasicColorChooser color {"Color"};
    BasicCheckbox replication {"Enable Replication"};
    MultiSelectDrag functions;
    

};
