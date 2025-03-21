/**
 * ConfigEditor to edit UI button bindings
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"


#include "BindingEditor.h"

class ButtonEditor : public BindingEditor
{
  public:
    ButtonEditor(class Supervisor* s);
    ~ButtonEditor();

    juce::String getTitle() override {return juce::String("Button Sets");}

    void prepare() override;
    void load() override;
    void save() override;
    void cancel() override;
    void revert() override;
    
    void objectSelectorSelect(int ordinal) override;
    void objectSelectorNew(juce::String newName) override;
    void objectSelectorDelete() override;
    void objectSelectorRename(juce::String) override;
    
    // BindingEditor overloads
    juce::String renderSubclassTrigger(OldBinding* b) override;
    bool isRelevant(class OldBinding* b) override;
    void addSubclassFields() override;
    void refreshSubclassFields(class OldBinding* b) override;
    void captureSubclassFields(class OldBinding* b) override;
    void resetSubclassFields() override;
    
  private:

    void refreshObjectSelector();
    void loadButtons(int index);
    void saveButtons(int index);
    
    juce::OwnedArray<class ButtonSet> buttons;
    // another copy for revert, don't need this as much for buttons
    juce::OwnedArray<class ButtonSet> revertButtons;
    int selectedButtons = 0;

    //Field* displayName = nullptr;
    YanInput displayName {"Display Name", 20};
    

};

