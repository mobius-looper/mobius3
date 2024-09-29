
#pragma once

#include "../../model/MobiusMidiState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"

#include "../../sync/Pulse.h"

#include "MidiRecorder.h"
#include "MidiPlayer.h"
#include "TrackEvent.h"

class MidiTrack
{
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

  private:

    class MobiusContainer* container = nullptr;
    class MidiTracker* tracker = nullptr;
    class ParameterFinder* finder = nullptr;
    class Pulsator* pulsator = nullptr;
    
    class MidiEventPool* midiPool = nullptr;
    class TrackEventPool* eventPool = nullptr;
    
    juce::OwnedArray<class MidiLoop> loops;
    int activeLoops = 0;
    int loopIndex = 0;
    TrackEventList events;
    MidiRecorder recorder {this};
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

    bool recording = false;
    bool synchronizing = false;
    bool durationMode = false;
    
    void advance(int newFrames);
    void advanceRecord(int newFrames);
    bool isExtending();
    void extend(int newFrames);
    void advanceAndLoop(int newFrames);
    void shift();
    
    void doEvent(TrackEvent* e);
    void doPulse(TrackEvent* e);
    
    void doRecord(class UIAction* a);
    void doRecord(class TrackEvent* e);
    bool needsRecordSync();
    void toggleRecording();
    void startRecording();
    void stopRecording();
    
    void doReset(class UIAction* a, bool full);
    void doOverdub(class UIAction* a);
    void doUndo(class UIAction* a);
    void doRedo(class UIAction* a);

    void doParameter(class UIAction* a);
    
};
