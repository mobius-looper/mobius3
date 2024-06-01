/**
 * Panel to edit plugin host parameter bidings.
 * These aren't really bindings because they don't have a trigger
 * but we rely on the same internal components to display them
 * and they're stored in the MobiusConfig Binding list.
 */

#pragma once

#include <JuceHeader.h>

#include "ConfigPanel.h"
#include "BindingPanel.h"

class HostPanel : public BindingPanel
{
  public:
    HostPanel(class ConfigEditor *);
    ~HostPanel();

    // BindingPanel overloads
    juce::String renderSubclassTrigger(Binding* b) override;
    bool isRelevant(class Binding* b) override;
    void addSubclassFields() override;
    void refreshSubclassFields(class Binding* b) override;
    void captureSubclassFields(class Binding* b) override;
    void resetSubclassFields() override;

  private:

};

