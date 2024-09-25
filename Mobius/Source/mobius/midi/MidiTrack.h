
#pragma once

#include "../../model/MobiusMidiState.h"
#include "../../model/Session.h"

#include "MidiPlayer.h"

class MidiTrack
{
    friend class MidiPlayer;
    
  public:

    MidiTrack(class MobiusContainer* c, class MidiTracker* t);
    ~MidiTrack();

    void configure(class Session::Track* def);
    
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
    class Pulsator* pulsator = nullptr;
    class MidiEventPool* eventPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    MidiPlayer player {this};
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
    
    void reclaim(class MidiSequence* seq);
    
    void doRecord(class UIAction* a);
    void startRecording(class UIAction* a);
    void stopRecording(class UIAction* a);
    
    void doReset(class UIAction* a);
    void doOverdub(class UIAction* a);
    
    void doParameter(class UIAction* a);
};
