/**
 * Manages a collection of ParameterForms and can swap between them
 * as things change.
 *
 * These differ from SessionFormCollection in the way the forms are build
 * dynamically rather than from static TreeForm definitions.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../parameter/ParameterForm.h"

class DynamicFormCollection : public juce::Component
{
  public:
    
    DynamicFormCollection();
    ~DynamicFormCollection() {}

    void resized() override;
    void paint(juce::Graphics& g) override;

    class ParameterForm* getForm(juce::String name);
    void addForm(juce::String name, class ParameterForm*);

    void show(class Provider* p, juce::String formName);

    void load(class Provider* p, class ValueSet* src);
    void save(class ValueSet* dest);
    void cancel();
    void decache();

    class ParameterForm* findFormWithLabel(class YanFieldLabel* l);
    
  private:

    class Provider* provider = nullptr;
    class ValueSet* sourceValues = nullptr;

    juce::OwnedArray<class ParameterForm> forms;
    juce::HashMap<juce::String,class ParameterForm*> formTable;
    class ParameterForm* currentForm = nullptr;
    
};

