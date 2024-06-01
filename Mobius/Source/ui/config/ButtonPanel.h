/**
 * Panel to edit UI button bindings
 */

#pragma once

#include <JuceHeader.h>

#include "ConfigPanel.h"
#include "BindingPanel.h"

class ButtonPanel : public BindingPanel
{
  public:
    ButtonPanel(class ConfigEditor *);
    ~ButtonPanel();

    // have to overload these since the DisplayButton model
    // isn't a Binding which is what BindingTable uses
    void load() override;
    void save() override;
    void cancel() override;
    
    // ObjectSelector overloads
    void selectObject(int ordinal) override;
    void newObject() override;
    void deleteObject() override;
    void revertObject() override;
    void renameObject(juce::String) override;
    
    // BindingPanel overloads
    juce::String renderSubclassTrigger(Binding* b) override;
    bool isRelevant(class Binding* b) override;
    void addSubclassFields() override;
    void refreshSubclassFields(class Binding* b) override;
    void captureSubclassFields(class Binding* b) override;
    void resetSubclassFields() override;
    juce::String getDisplayName(class Binding* b) override;
    
  private:

    void loadButtons(int index);
    void saveButtons(int index);
    class DisplayButton* getDisplayButton(class Binding* binding);
    
    juce::OwnedArray<class ButtonSet> buttons;
    // another copy for revert, don't need this as much for buttons
    juce::OwnedArray<class ButtonSet> revertButtons;
    int selectedButtons = 0;

    Field* displayName = nullptr;
    

};

