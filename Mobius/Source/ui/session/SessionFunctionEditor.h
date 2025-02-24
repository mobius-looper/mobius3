/**
 * A tree/form combo that edits the global session parameters.
 */

#pragma once

#include <JuceHeader.h>

#include "DynamicTreeForms.h"

class SessionFunctionEditor : public juce::Component
{
  public:

    SessionFunctionEditor();
    ~SessionFunctionEditor() {}

    // SessionEditor Interface

    void initialize(class Provider* p);
    
    void load(class ValueSet* set);
    void save(ValueSet* values);
    void cancel();
    void decacheForms();

    void showInitial(juce::String name);
    
    void resized() override;
    
  private:

    class Provider* provider = nullptr;
    class ValueSet* values = nullptr;

    DynamicTreeForms forms;

};


