/**
 * Dynamic form containing fields for editing parameter symbols.
 *
 * The parameters to edit are injected from above using several interfaces.
 * Once constructed, field values are read from and saved to a ValueSet.
 *
 * Awareness of the surrounding context must be kept to a minimum to enable
 * it's use in several places.
 *
 * This is not specific to Session editing so it could be moved, but that is it's
 * primary use.
 *
 * There is some form wrapper support like a title which should be kept to a minimum
 * and be optional.  May want to factor this out.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanForm.h"
#include "../common/YanParameter.h"

class ParameterForm : public juce::Component
{
  public:

    ParameterForm();
    ~ParameterForm() {}

    /**
     * Forms may have an optional title which is displayed above the form fields.
     * When there is a title the fields are inset.
     */
    void setTitle(juce::String s);

    /**
     * Adjust the insets.
     */
    void setTitleInset(int x);
    void setFormInset(int x);

    /**
     * Add a list of editing fields for parameter symbols.
     * The fields are added in the same order as the array.
     */
    void add(juce::Array<class Symbol*>& symbols);

    /**
     * Add a random field not necessarily associated with a symbol.
     */
    void add(class YanField* f);

    /**
     * Add a spacer.
     */
    void addSpacer();

    /**
     * Add form fields from a form definition.
     */
    void add(class TreeForm* formdef);
    
    /**
     * Load the values of symbol parameter fields from the value set.
     * Only fields added with Symbols can be loaded this way.
     */
    void load(class ValueSet* values);

    /**
     * Save the values of symbol parameter fields to the value set.
     */
    void save(class ValueSet* values);

    //
    // Juce
    //

    void resized() override;
    void paint(juce::Graphics& g) override;

  private:

    juce::String title;
    int titleInset = 20;
    int formInset = 100;
    
    YanForm form;
    juce::OwnedArray<class YanParameter> parameters;
    juce::OwnedArray<class YanField> others;
    
};
    
