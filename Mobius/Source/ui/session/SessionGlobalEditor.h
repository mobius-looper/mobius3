/**
 * A tree/form combo that edits the global session parameters.
 */

#pragma once

#include <JuceHeader.h>

#include "SymbolTree.h"
#include "SessionEditorTree.h"
#include "SessionFormCollection.h"


class SessionGlobalEditor : public juce::Component, public SymbolTree::Listener
{
  public:

    SessionGlobalEditor();
    ~SessionGlobalEditor() {}

    // SessionEditor Interface

    void initialize(class Provider* p);
    
    void load(class ValueSet* set);
    void save(ValueSet* values);

    void showInitial(juce::String name);
    
    // Subcomponent Communication

    void symbolTreeClicked(SymbolTreeItem* item) override;

    // Juce
    
    void resized() override;
    
  private:

    class Provider* provider = nullptr;
    class ValueSet* values = nullptr;

    SessionEditorTree tree;
    SessionFormCollection forms;

};


