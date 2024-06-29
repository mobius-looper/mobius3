/**
 * Base class for binding editing panels.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/Field.h"
#include "../common/Form.h"
#include "../../test/BasicButtonRow.h"

#include "NewConfigEditor.h"
#include "BindingTable.h"
#include "BindingTargetPanel.h"

class BindingEditor : public NewConfigEditor,
                      public BindingTable::Listener,
                      public Field::Listener
{
  public:

    // Subclasses must implement these
    virtual juce::String renderSubclassTrigger(class Binding* b) = 0;
    virtual bool isRelevant(class Binding* b) = 0;
    virtual void addSubclassFields() = 0;
    virtual void refreshSubclassFields(class Binding* b) = 0;
    virtual void captureSubclassFields(class Binding* b) = 0;
    virtual void resetSubclassFields() = 0;

    BindingEditor();
    virtual ~BindingEditor();

    // ConfigEditor
    virtual void load() override;
    virtual void save() override;
    virtual void cancel() override;
    virtual void revert() override;
    
    void objectSelectorSelect(int ordinal) override;
    void objectSelectorNew(juce::String newName) override;
    void objectSelectorDelete() override;
    void objectSelectorRename(juce::String) override;
    
    // Component
    void resized() override;

    // BindingTable
    juce::String renderTriggerCell(class Binding* b) override;
    void bindingSelected(class Binding* b) override;
    void bindingUpdate(class Binding* b) override;
    void bindingDelete(class Binding* b) override;
    class Binding* bindingNew() override;

    // Field
    void fieldChanged(Field* field) override;
    
  protected:

    BindingTable bindings;
    BindingTargetPanel targets;
    Form form;
    Field* scope = nullptr;
    Field* arguments = nullptr;
    int maxTracks = 0;
    int maxGroups = 0;
    juce::ToggleButton activeButton {"Active"};
    juce::ToggleButton mergeButton {"Merge with others"};
    BasicButtonRow activationButtons;
    
    void initForm();
    void resetForm();
    
  private:

    void refreshObjectSelector();
    void render();
    void rebuildTable();

    void refreshForm(class Binding* b);
    void captureForm(class Binding* b);

    // start making this more like Preset and other multi-object panels
    juce::OwnedArray<class BindingSet> bindingSets;
    juce::OwnedArray<class BindingSet> revertBindingSets;
    int selectedBindingSet = 0;

    void loadBindingSet(int index);
    void saveBindingSet(int index);
    void saveBindingSet(class BindingSet* dest);


};
