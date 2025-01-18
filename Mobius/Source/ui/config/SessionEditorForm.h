/**
 * Dynamic form generated for the session editor containing
 * fields for a list of categorized symbols.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanForm.h"
#include "../common/YanField.h"

class SessionEditorForm : public juce::Component
{
  public:

    SessionEditorForm();
    ~SessionEditorForm() {}

    void resized() override;
    void paint(juce::Graphics& g) override;

    void load(juce::String category, juce::Array<class Symbol*>& symbols);

  private:

    juce::String category;
    YanForm form;
    juce::OwnedArray<YanField> fields;
    
};
    
