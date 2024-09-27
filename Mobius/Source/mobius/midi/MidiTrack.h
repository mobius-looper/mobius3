
#pragma once

#include "../../model/MobiusMidiState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"

#include "../../sync/Pulse.h"

#include "MidiPlayer.h"
#include "TrackEvent.h"

class MidiTrack
{
    friend class MidiPlayer;
    
  public:

    MidiTrack(class MobiusContainer* c, class MidiTracker* t);
    ~MidiTrack();

    void configure(class Session::Track* def);
    void reset();
    
    bool isRecording();
    void midiEvent(class MidiEvent* e);
    void processAudioStream(class MobiusAudioStream* argStream);
    void doAction(class UIAction* a);
    void doQuery(class Query* q);
    
    // the track number in "reference space"
    int number = 0;
    // the track index within the MidiTracker, need this?
    int index = 0;
    

    void refreshState(class MobiusMidiState::Track* state);

  protected:

    class MidiSequence* getPlaySequence() {
        return playing;
    }

    int getLoopFrames() {
        return frames;
    }

  private:

    class MobiusContainer* container = nullptr;
    class MidiTracker* tracker = nullptr;
    class ParameterFinder* finder = nullptr;
    class Pulsator* pulsator = nullptr;
    class MidiEventPool* midiPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    class TrackEventPool* eventPool = nullptr;
    
    TrackEventList events;
    MidiPlayer player {this};

    // sync status
    Pulse::Source syncSource = Pulse::SourceNone;
    int syncLeader = 0;

    // loop state
    int frame = 0;
    int frames = 0;
    int cycle = 0;
    int cycles = 0;
    int subcycle = 0;
    int subcycles = 0;
    
    MobiusMidiState::Mode mode = MobiusMidiState::ModeReset;
    bool overdub = false;
    bool reverse = false;
    bool mute = false;
    bool pause = false;
    
    int input = 127;
    int output = 127;
    int feedback = 127;
    int pan = 64;

    class MidiSequence* recording = nullptr;
    class MidiSequence* playing = nullptr;
    bool synchronizing = false;

    void advance(int newFrames);
    void doEvent(TrackEvent* e);
    void doPulse(TrackEvent* e);
    
    void reclaim(class MidiSequence* seq);
    
    void doRecord(class UIAction* a);
    void doRecord(class TrackEvent* e);
    bool needsRecordSync();
    void toggleRecording();
    void startRecording();
    void stopRecording();
    
    void doReset(class UIAction* a);
    void doOverdub(class UIAction* a);
    
    void doParameter(class UIAction* a);
};
