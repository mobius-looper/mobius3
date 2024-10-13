
#pragma once

#include "../../util/Util.h"
#include "../../model/MobiusMidiState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"

#include "../../sync/Pulse.h"

#include "MidiRecorder.h"
#include "MidiPlayer.h"
#include "MidiFunctions.h"

class MidiTrack
{
    friend class TrackScheduler;
    friend class MidiFunctions;
    
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

    //
    // stimuli
    //
    
    void doAction(class UIAction* a);
    void doActionNow(class UIAction* a);
    void doQuery(class Query* q);
    void processAudioStream(class MobiusAudioStream* argStream);

    void noteOn(class MidiEvent* e);
    void noteOff(class MidiEvent* e);
    void midiEvent(class MidiEvent* e);

    //
    // Support for function handlers
    //

    
    //
    // Support for Recorder
    //

    MobiusMidiState::Mode getMode();
    
    class MidiEvent* getHeldNotes();
    class MidiEvent* copyNote(class MidiEvent* src);

  protected:

    //
    // TrackScheduler Interface
    //

    int getLoopFrames();
    int getFrame();
    int getCycleFrames();
    int getCycles();
    int getSubcycles();
    int getModeStartFrame();
    int getModeEndFrame();
    
    void alert(const char* msg);
    
    void endRecording(class UIAction* a);

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
    MidiFunctions functions {this};
    juce::Array<MobiusMidiState::Region> regions;
    int activeRegion = -1;
    
    // state
    MobiusMidiState::Mode mode = MobiusMidiState::ModeReset;
    bool synchronizing = false;
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
    void advance(int newFrames);
    void advancePlayer(int newFrames);
    void shift();
    void shiftMultiply(bool unrounded);
    void endInsert(bool unrounded);
    void shiftInsert(bool unrounded);
    
    // actions/events
    void doEvent(TrackEvent* e);
    void doPulse(TrackEvent* e);
    void doParameter(class UIAction* a);
    void doReset(class UIAction* a, bool full);

    //
    // Function Handlers
    //
    
    void doRecord(class UIAction* a);
    void doRecord(class TrackEvent* e);
    bool needsRecordSync();
    void toggleRecording();
    void startRecording();
    void stopRecording();

    void resetRegions();
    void startOverdubRegion();
    void stopOverdubRegion();
    void advanceRegion(int frames);
    
    void doOverdub(class UIAction* a);
    bool inRecordingMode();
    void doUndo(class UIAction* a);
    void doRedo(class UIAction* a);

    void doSwitch(class UIAction* a, int delta);
    class TrackEvent* newSwitchEvent(int target, int framee);
    QuantizeMode convert(SwitchQuantize squant);
    int getQuantizeFrame(SwitchQuantize squant);
    void doSwitch(class TrackEvent* e);
    void doSwitchNow(int target);
    void finishRecordingMode();

    int getQuantizeFrame(QuantizeMode qmode);
    int getRoundedFrame();
    void doRound(class TrackEvent* e);
    void doInsert(class UIAction* a);
    void doInsert(class TrackEvent* e);
    void doInsertNow();
    
    void doMute(class UIAction* a);
    void doMute(class TrackEvent* e);
    void doMuteNow();
    
    int getQuantizeFrame(SymbolId func, QuantizeMode qmode);
    void doReplace(class UIAction* a);
    void doReplace(class TrackEvent* e);
    void doReplaceNow();

    void doDump(class UIAction* a);

};
