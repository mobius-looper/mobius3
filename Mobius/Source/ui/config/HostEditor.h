/**
 * Panel to edit plugin host parameter bidings.
 * These aren't really bindings because they don't have a trigger
 * but we rely on the same internal components to display them
 * and they're stored in the MobiusConfig Binding list.
 */

#pragma once

#include <JuceHeader.h>

#include "BindingEditor.h"

class HostEditor : public BindingEditor
{
  public:
    HostEditor(class Supervisor* s);
    ~HostEditor();

    juce::String getTitle() override {return "Plugin Parameters";}

    juce::String renderSubclassTrigger(OldBinding* b) override;
    bool isRelevant(class OldBinding* b) override;
    void addSubclassFields() override;
    void refreshSubclassFields(class OldBinding* b) override;
    void captureSubclassFields(class OldBinding* b) override;
    void resetSubclassFields() override;

  private:

};

