/**
 * Subcomponent of SessionEditor for editing each track configuration.
 */

#pragma once

#include <JuceHeader.h>

#include "../../model/Session.h"
#include "../script/TypicalTable.h"

class SessionTrackEditor : public juce::Component,
                           public TypicalTable::Listener
{
  public:

    /**
     * State maintained for each track.
     * Takes ownership of the Session::Track
     */
    class TrackState {
      public:
        TrackState(Session::Track* t);
        
        Session::Track* getTrack();
        Session::Track* stealTrack();

        // these are created on demand when clicking on a track in the table
        class SessionTrackForms* getForms();
        void setForms(class SessionTrackForms* f);

      private:
        
        std::unique_ptr<Session::Track> trackdef;
        std::unique_ptr<class SessionTrackForms> forms;
    };

    SessionTrackEditor();
    ~SessionTrackEditor();

    void initialize(class Provider* p);
    void load(class Session* src);
    void save(class Session* dest);
    void cancel();
    void decacheForms();
    
    void resized() override;

    void typicalTableChanged(class TypicalTable* t, int row) override;

    // SessionTrackTable callbacks
    void renameTrack(int index, juce::String newName);
    void moveTrack(int sourceRow, int desiredRow);
    void addTrack(Session::TrackType type);
    void deleteTrack(int index);
    void bulkReconcile(int audioCount, int midiCount);
    
  private:

    class Provider* provider = nullptr;
    int currentTrack = 0;
    
    std::unique_ptr<class SessionTrackTable> table;
    juce::OwnedArray<class TrackState> states;
    
    TrackState* getState(int index);
    void show(int index);
    void reconcileTrackCount(Session::TrackType type, int required);
    void addState(Session::TrackType type);
    void deleteState(int index);
    
};

        
