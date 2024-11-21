/**
 * Manager of all MIDI tracks
 *
 * This roughly corresponds to Mobius as the manager of audio tracks.
 */

#pragma once

#include <JuceHeader.h>

#include "../track/TrackEngine.h"

class MidiEngine : public TrackEngine
{
  public:

    MidiEngine(class TrackManager* manager);
    ~MidiEngine();

    // TrackEngine

    void initialize(int baseNumber, class Session* session);
    void loadSession(class Session* session);
    class AbstractTrack* getTrack(int number);
    class TrackProperties getTrackProperties(int number);
    void trackNotification(NotificationId notification, class TrackProperties& props);
    void midiEvent(class MidiEvent* e);
    void processAudioStream(class MobiusAudioStream* stream);
    int getTrackGroup(int number);
    void doGlobal(class UIAction* a);
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);
    void loadLoop(class MidiSequence* seq, int track, int loop);
    void refreshState(class MobiusState* state);

  private:

    class TrackManager* manager = nullptr;

    int baseNumber = 0;
    juce::OwnedArray<class MidiTrack> midiTracks;

    
};

        
    
 
