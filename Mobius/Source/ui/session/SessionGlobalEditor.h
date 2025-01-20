/**
 * A tree/form combo that edits the global session parameters.
 */

#pragma once

#include <JuceHeader.h>

#include "SessionTreeForms.h"

class SessionGlobalEditor : public juce::Component
{
  public:

    SessionGlobalEditor();
    ~SessionGlobalEditor() {}

    // SessionEditor Interface

    void initialize(class Provider* p);
    
    void load(class ValueSet* set);
    void save(ValueSet* values);

    void showInitial(juce::String name);
    
    void resized() override;
    
  private:

    class Provider* provider = nullptr;
    class ValueSet* values = nullptr;

    SessionTreeForms forms;

};


