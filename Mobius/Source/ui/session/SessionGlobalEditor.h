/**
 * A tree/form combo that edits the global session parameters.
 */

#pragma once

#include <JuceHeader.h>

#include "../parameter/ParameterTreeForms.h"

class SessionGlobalEditor : public ParameterTreeForms,
                            public ParameterFormCollection::Factory,
                            public YanParameter::Listener
{
  public:

    constexpr static const char* TreeName = "sessionGlobal";

    SessionGlobalEditor();
    ~SessionGlobalEditor() {}

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


