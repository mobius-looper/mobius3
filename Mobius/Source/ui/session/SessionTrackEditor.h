/**
 * Subcomponent of SessionEditor for editing each track configuration.
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"
#include "SymbolTree.h"

class SessionTrackEditor : public juce::Component,
                           public TypicalTable::Listener,
                           public SymbolTree::Listener
{
  public:

    SessionTrackEditor(class Provider* p);
    ~SessionTrackEditor();

    void loadSymbols();
    void load();
    
    void resized() override;

    void typicalTableChanged(class TypicalTable* t, int row) override;
    
  private:

    class Provider* provider = nullptr;

    std::unique_ptr<class SessionTrackTable> tracks;
    std::unique_ptr<class SessionTrackTrees> trees;
    
};

        
