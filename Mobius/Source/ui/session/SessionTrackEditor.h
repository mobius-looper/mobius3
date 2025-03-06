/**
 * Subcomponent of SessionEditor for editing each track configuration.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Session.h"
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
    void deleteTrack(int index);
    void bulkReconcile(int audioCount, int midiCount);
    
  private:

    class Provider* provider = nullptr;
    class Session* session = nullptr;
    int currentTrack = 0;
    
    std::unique_ptr<class SessionTrackTable> tracks;
    juce::OwnedArray<class SessionTrackForms> trackForms;
    
    SessionTreeForms audioForms;
    SessionTreeForms midiForms;

    void loadForms(int index);
    void saveForms(int index);
    
    Session::Track* findMatchingTrack(juce::Array<Session::Track*>& tracks, int id);
    void deleteRemaining(juce::Array<Session::Track*>& tracks);
    
};

        
