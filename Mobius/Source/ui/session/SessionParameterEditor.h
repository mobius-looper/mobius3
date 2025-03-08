/**
 * A tree/form combo that edits the full set of deafult track parameters.
 */

#pragma once

#include <JuceHeader.h>

#include "../common/YanParameter.h"
#include "../parameter/ParameterTreeForms.h"
#include "../parameter/ParameterFormCollection.h"

class SessionParameterEditor : public ParameterTreeForms,
                               public ParameterFormCollection::Factory,
                               public YanParameter::Listener
{
  public:

    SessionParameterEditor();
    ~SessionParameterEditor() {}

    // SessionEditor Interface

    void initialize(class Provider* p, class SessionEditor* se);
    
    void load(class ValueSet* set);
    void save(ValueSet* values);
    void cancel();
    void decacheForms();

    class ParameterForm* parameterFormCollectionCreate(juce::String formid) override;

    void yanParameterChanged(YanParameter* p) override;
    
  private:

    class Provider* provider = nullptr;
    class SessionEditor* editor = nullptr;
    class ValueSet* values = nullptr;

};


