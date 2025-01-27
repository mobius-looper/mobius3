/**
 * Subcomponent of SessionEditor for editing each track configuration.
 */

#pragma once

#include <JuceHeader.h>

#include "../script/TypicalTable.h"
#include "SessionTreeForms.h"

class SessionTrackEditor : public juce::Component,
                           public TypicalTable::Listener
{
  public:

    SessionTrackEditor();
    ~SessionTrackEditor();

    void initialize(class Provider* p);
    void load(class Session* src);
    void save(class Session* dest);
    void cancel();
    void decacheForms();
    
    void resized() override;

    void typicalTableChanged(class TypicalTable* t, int row) override;
    void move(int sourceRow, int desiredRow);
    void addTrack(Session::TrackType type);
    void deleteTrack(int number);
    void bulkReconcile(int audioCount, int midiCount);
    
  private:

    class Provider* provider = nullptr;
    class Session* session = nullptr;
    int currentTrack = 1;
    
    std::unique_ptr<class SessionTrackTable> tracks;

    SessionTreeForms audioForms;
    SessionTreeForms midiForms;

    void loadForms(int number);
    void saveForms(int number);
    
};

        
