/**
 * Manages a collection of ParameterForms and can swap between them
 * as things change.
 *
 * The SessionEditor has two of these, one for the Global parameters and
 * one for the Track parameters.
 *
 * Forms are created dynamically as selections are made in a corresponding
 * ParameterTree.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../session/ParameterForm.h"

class ParameterFormCollection : public juce::Component
{
  public:
    
    ParameterFormCollection();
    ~ParameterFormCollection() {}

    void resized() override;
    void paint(juce::Graphics& g) override;

    class ParameterForm* getForm(juce::String name);
    void addForm(juce::String name, class ParameterForm*);

    void show(class Provider* p, juce::String formName);

    void load(class Provider* p, class ValueSet* src);
    void save(class ValueSet* dest);
    void cancel();
    void decache();
    
  private:

    class ValueSet* sourceValues = nullptr;

    juce::OwnedArray<class ParameterForm> forms;
    juce::HashMap<juce::String,class ParameterForm*> formTable;
    class ParameterForm* currentForm = nullptr;
    
};

