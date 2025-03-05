/**
 * A tree/form combo that edits the global session parameters.
 */

#pragma once

#include <JuceHeader.h>

#include "ParameterTreeForms.h"

class SessionGlobalEditor : public ParameterTreeForms,
                            public SymbolTree::Listener,
                            public ParameterFormCollection::Factory
{
  public:

    constexpr static const char* TreeName = "sessionGlobal";

    SessionGlobalEditor();
    ~SessionGlobalEditor() {}

    // SessionEditor Interface

    void initialize(class Provider* p);
    
    void load(class ValueSet* set);
    void save(ValueSet* values);
    void cancel();
    void decacheForms();

    void showInitial(juce::String name);

    void symbolTreeClicked(class SymbolTreeItem* item) override;
    class ParameterForm* parameterFormCollectionCreate(juce::String formid) override;
    
  private:

    class Provider* provider = nullptr;
    class ValueSet* values = nullptr;


};


