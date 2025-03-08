/**
 * Dynamic form containing fields for editing the contents of a ValueSet
 * whose contents are not defined with Symbols and ParameterProperties.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanField.h"
#include "../common/YanForm.h"
#include "../common/YanParameter.h"
#include "ValueSetField.h"

class ValueSetForm : public juce::Component
{
  public:

    ValueSetForm();
    ~ValueSetForm() {}

    /**
     * Adjust the insets.
     */
    void setTitleInset(int x);
    void setFormInset(int x);

    /**
     * Build the form from a static definition
     */
    void build(class Provider* p, class Form* f);

    /**
     * Load the values of the Fields from the value set.
     */
    void load(class ValueSet* values);

    /**
     * Save the fields to a value set.
     */
    void save(class ValueSet* values);

    //
    // Juce
    //

    void resized() override;
    void paint(juce::Graphics& g) override;

    void forceResize();

  private:

    juce::String title;

    // this gives it a little border between the title and the container
    int titleInset = 20;

    // this needs to be large enough to include the title inset plus the
    // title height 
    int formInset = 42;
    
    YanForm form;
    juce::OwnedArray<class ValueSetField> fields;
    juce::OwnedArray<class YanField> others;
    
};
    
