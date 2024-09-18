
#pragma once

#include "../../model/MobiusMidiState.h"

class MidiTrack
{
  public:

    MidiTrack(class MidiTracker* t);
    ~MidiTrack();
    void initialize();

    bool isRecording();
    void midiEvent(class MidiEvent* e);
    void processAudioStream(class MobiusAudioStream* argStream);
    void doAction(class UIAction* a);
    void doQuery(class Query* q);
    
    int index = 0;

    void refreshState(class MobiusMidiState::Track* state);

  private:

    class MidiTracker* tracker = nullptr;
    class MidiEventPool* eventPool = nullptr;
    class MidiSequencePool* sequencePool = nullptr;
    
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

    void reclaim(class MidiSequence* seq);
    
    void doRecord(class UIAction* a);
    void startRecording(class UIAction* a);
    void stopRecording(class UIAction* a);
    
    void doReset(class UIAction* a);
    void doOverdub(class UIAction* a);
    
    void doParameter(class UIAction* a);
};
