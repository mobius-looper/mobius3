/**
 * Base class for binding editing panels.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"
#include "../common/YanForm.h"
#include "../common/BasicButtonRow.h"

#include "OldBindingTable.h"
#include "TargetSelectorWrapper.h"
#include "ConfigEditor.h"

class OldBindingEditor : public ConfigEditor,
                      public OldBindingTable::Listener,
                      public TargetSelectorWrapper::Listener,
                      public YanInput::Listener,
                      public YanCombo::Listener
{
  public:

    // Subclasses must implement these
    virtual juce::String renderSubclassTrigger(class OldBinding* b) = 0;
    virtual bool isRelevant(class OldBinding* b) = 0;
    virtual void addSubclassFields() = 0;
    virtual bool wantsCapture() {return false;}
    virtual bool wantsPassthrough() {return false;}
    virtual void refreshSubclassFields(class OldBinding* b) = 0;
    virtual void captureSubclassFields(class OldBinding* b) = 0;
    virtual void resetSubclassFields() = 0;

    // subclass may call this if it wants an object selector
    void setInitialObject(juce::String name);

    // subclass may call this if it wants to append a release checkbox
    void addRelease();

    OldBindingEditor(class Supervisor* s);
    virtual ~OldBindingEditor();

    bool isCapturing();
    bool isCapturePassthrough();
    void showCapture(juce::String& stuff);

    // Component
    void resized() override;

    // ConfigEditor
    virtual void load() override;
    virtual void save() override;
    virtual void cancel() override;
    virtual void revert() override;
    
    void objectSelectorSelect(int ordinal) override;
    void objectSelectorNew(juce::String newName) override;
    void objectSelectorDelete() override;
    void objectSelectorRename(juce::String) override;
    
    // BindingTable::Listener
    juce::String renderTriggerCell(class OldBinding* b) override;
    void bindingSelected(class OldBinding* b) override;
    void bindingDeselected() override;
    void bindingUpdate(class OldBinding* b) override;
    void bindingDelete(class OldBinding* b) override;
    class OldBinding* bindingNew() override;
    class OldBinding* bindingCopy(class OldBinding* b) override;

    // BindingTargetSelector::Listener
    void bindingTargetClicked() override;

    // YanField Listeners
    void yanInputChanged(class YanInput* i) override;
    void yanComboSelected(class YanCombo* c, int selection) override;
    
  protected:
    
    OldBindingTable bindings;
    TargetSelectorWrapper targets;
    juce::String initialObject;

    YanForm form;
    YanCombo scope {"Scope"};
    YanInput arguments {"Arguments", 20};
    YanCheckbox capture {"Capture"};
    YanInput annotation {"", 5, true};
    YanCheckbox passthrough {"Active"};
    YanCheckbox release {"Release"};
    
    int maxTracks = 0;
    //int maxGroups = 0;
    //juce::ToggleButton activeButton {"Active"};
    juce::ToggleButton overlayButton {"Overlay"};
    BasicButtonRow activationButtons;
    
    void initForm();
    void resetForm();
    void resetFormAndTarget();

    void formChanged();
    void targetChanged();
    
  private:

    void refreshScopeNames();
    void refreshObjectSelector();
    void render();
    void rebuildTable();

    void refreshForm(class OldBinding* b);
    void captureForm(class OldBinding* b, bool includeTarget);
    
    // start making this more like Preset and other multi-object panels
    juce::OwnedArray<class OldBindingSet> bindingSets;
    juce::OwnedArray<class OldBindingSet> revertBindingSets;
    int selectedBindingSet = 0;

    void loadBindingSet(int index);
    void saveBindingSet(int index);
    void saveBindingSet(class OldBindingSet* dest);


};
