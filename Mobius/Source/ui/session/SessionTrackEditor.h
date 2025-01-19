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

    SessionTrackEditor();
    ~SessionTrackEditor();

    void initialize(class Provider* p);
    void load(class Session* src);
    void save(class Session* dest);
    
    void resized() override;

    void typicalTableChanged(class TypicalTable* t, int row) override;
    void symbolTreeClicked(class SymbolTreeItem* item) override;
    
  private:

    class Provider* provider = nullptr;
    class Session* session = nullptr;
    int currentTrack = 1;
    
    std::unique_ptr<class SessionTrackTable> tracks;
    std::unique_ptr<class SessionTrackTrees> trees;
    std::unique_ptr<class SessionFormCollection> forms;

    void loadForms(int number);
    void saveForms(int number);
    
};

        
