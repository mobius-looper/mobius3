/**
 * Manages a collection of ParameterForms and can swap between them
 * as things change.
 *
 * Forms have an identifier whose meaning is undefined, but are typically
 * the names of static TreeForm definitions or the names of parameter categories.
 *
 * How the forms are constructed is left to the owner.  Pre-built forms may be added
 * befor the collection is used, or a Factory class may be given that abstracts
 * the construction of new forms as they are requested.
 */

#pragma once

#include <JuceHeader.h>

class ParameterFormCollection : public juce::Component
{
  public:
    
    ParameterFormCollection();
    ~ParameterFormCollection();

    class Factory {
      public:
        virtual ~Factory() {}
        virtual class ParameterForm* parameterFormCollectionCreate(juce::String formid) = 0;
    };

    void initialize(class Provider* p, class Factory* f, class ValueSet* values);

    void add(juce::String formName, class ParameterForm* form);
    
    // may call back to the factory if the form was not found
    void show(juce::String formName);

    void load(class ValueSet* src);
    void save(class ValueSet* dest);
    void cancel();
    void decache();
    
    void resized() override;
    void paint(juce::Graphics& g) override;
    
  private:

    class Provider* provider = nullptr;
    Factory* factory = nullptr;
    class ValueSet* valueSet = nullptr;

    juce::OwnedArray<class ParameterForm> forms;
    juce::HashMap<juce::String,class ParameterForm*> formTable;
    class ParameterForm* currentForm = nullptr;
    
};

