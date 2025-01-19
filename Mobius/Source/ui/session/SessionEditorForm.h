// OBSOLETE: DELTEE

/**
 * Dynamic form generated for the session editor containing
 * fields for a list of categorized symbols.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanForm.h"
#include "../common/YanParameter.h"

class SessionEditorForm : public juce::Component
{
  public:

    SessionEditorForm();
    ~SessionEditorForm() {}

    void resized() override;
    void paint(juce::Graphics& g) override;

    void initialize(juce::String category, juce::Array<class Symbol*>& symbols);
    void load(class ValueSet* values);
    void save(class ValueSet* values);

  private:

    juce::String category;
    YanForm form;
    juce::OwnedArray<class YanParameter> fields;
    
};
    
