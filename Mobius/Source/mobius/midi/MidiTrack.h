
#pragma once

#include "../../util/Util.h"
#include "../../model/MobiusMidiState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"

#include "../../sync/Pulse.h"

#include "MidiRecorder.h"
#include "MidiPlayer.h"
#include "TrackScheduler.h"

class MidiTrack
{
    friend class TrackScheduler;

  public:

    //
    // Configuration
    //

    MidiTrack(class MobiusContainer* c, class MidiTracker* t);
    ~MidiTrack();
    void configure(class Session::Track* def);
    void reset();

    class MidiTracker* getTracker() {
        return tracker;
    }
    
    // the track number in "reference space"
    int number = 0;
    // the track index within the MidiTracker, need this?
    int index = 0;
    
    //
    // State
    //
    
    bool isRecording();
    void refreshState(class MobiusMidiState::Track* state);
    void refreshImportant(class MobiusMidiState::Track* state);
    MobiusMidiState::Mode getMode();

    //
    // stimuli
    //
    
    void doAction(class UIAction* a);
    void doQuery(class Query* q);
    void processAudioStream(class MobiusAudioStream* argStream);

    void noteOn(class MidiEvent* e);
    void noteOff(class MidiEvent* e);
    void midiEvent(class MidiEvent* e);

    //
    // Support for Recorder
    //
    
    class MidiEvent* getHeldNotes();
    class MidiEvent* copyNote(class MidiEvent* src);

  protected:

    //
    // TrackScheduler Interface
    //

    void alert(const char* msg);
    int getLoopIndex();
    int getLoopCount();
    int getLoopFrames();
    int getFrame();
    int getFrames();
    int getCycleFrames();
    int getCycles();
    int getSubcycles();
    int getModeStartFrame();
    //int getRoundingFrames();
    int getModeEndFrame();
    int extendRounding();

    // mode transitions
    
    void startRecord();
    void finishRecord();

    void startMultiply();
    void finishMultiply();

    void startInsert();
    void finishInsert();

    void startReplace();
    void finishReplace();

    void toggleOverdub();
    void toggleMute();

    void finishSwitch(int target);
    
    void advance(int newFrames);
    
  private:

    class MobiusContainer* container = nullptr;
    class Valuator* valuator = nullptr;
    class Pulsator* pulsator = nullptr;
    class MidiTracker* tracker = nullptr;
    class MidiPools* pools = nullptr;

    // loops
    juce::OwnedArray<class MidiLoop> loops;
    int loopCount = 0;
    int loopIndex = 0;

    // the meat
    MidiRecorder recorder {this};
    MidiPlayer player {this};
    TrackScheduler scheduler {this};
    
    juce::Array<MobiusMidiState::Region> regions;
    int activeRegion = -1;
    
    // state
    MobiusMidiState::Mode mode = MobiusMidiState::ModeReset;
    bool overdub = false;
    bool reverse = false;
    bool mute = false;
    bool pause = false;
    
    int input = 127;
    int output = 127;
    int feedback = 127;
    int pan = 64;
    int subcycles = 4;

    // advance
    void advancePlayer(int newFrames);
    void shift();
    void shiftMultiply(bool unrounded);
    void shiftInsert(bool unrounded);
    bool checkMultiplyTermination();
    
    // actions/events
    void doParameter(class UIAction* a);
    void doReset(bool full);
    void doUndo();
    void doRedo();
    void doDump();

    //
    // Function Handlers
    //
    
    void resetRegions();
    void startOverdubRegion();
    void stopOverdubRegion();
    void advanceRegion(int frames);
    
    bool inRecordingMode();
    void unroundedMultiply();
    void unroundedInsert();

};
