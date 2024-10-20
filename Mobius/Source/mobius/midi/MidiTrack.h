
#pragma once

#include "../../util/Util.h"
#include "../../model/MobiusMidiState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"

#include "../../sync/Pulse.h"

#include "AbstractTrack.h"
#include "MidiRecorder.h"
#include "MidiPlayer.h"
#include "TrackScheduler.h"
#include "ActionTransformer.h"

class MidiTrack : public AbstractTrack
{
    friend class ActionTransformer;
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

    //
    // AbstractTrack for ActionTransformer and TrackScheduler
    //

    int getNumber() override {
        return number;
    }
    
    void alert(const char* msg) override;
    MobiusMidiState::Mode getMode() override;
    int getLoopIndex() override;
    int getLoopCount() override;
    int getLoopFrames() override;
    int getFrame() override;
    int getCycleFrames() override;
    int getCycles() override;
    int getSubcycles() override;
    int getModeStartFrame() override;
    int getModeEndFrame() override;
    int extendRounding() override;

    // mode transitions
    
    void startRecord() override;
    void finishRecord() override;

    void startMultiply() override;
    void finishMultiply() override;
    void unroundedMultiply() override;

    void startInsert() override;
    int extendInsert() override;
    void finishInsert() override;
    void unroundedInsert() override;

    void toggleOverdub() override;
    void toggleMute() override;
    void toggleReplace() override;

    bool finishSwitch(int target) override;
    
    // simple one-shot actions
    void doParameter(class UIAction* a) override;
    void doReset(bool full) override;
    void doUndo() override;
    void doRedo() override;
    void doDump() override;
    void doInstantMultiply(int n) override;
    void doInstantDivide(int n) override;
    
    bool isExtending() override;
    void advance(int newFrames) override;
    void loop() override;
    
  protected:

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
    ActionTransformer transformer {this, &scheduler};
    
    juce::Array<MobiusMidiState::Region> regions;
    int activeRegion = -1;
    
    // state
    MobiusMidiState::Mode mode = MobiusMidiState::ModeReset;
    bool overdub = false;
    bool mute = false;
    bool reverse = false;
    bool pause = false;
    
    int input = 127;
    int output = 127;
    int feedback = 127;
    int pan = 64;
    int subcycles = 4;
    int inputMonitor = 0;
    int inputDecay = 0;
    int outputMonitor = 0;
    int outputDecay = 0;
    
    // advance
    void advancePlayer(int newFrames);
    void shift(bool unrounded);
    bool isMultiplyEndScheduled();
    void doMultiplyEarlyTermination();
    
    //
    // Function Handlers
    //
    
    void resetRegions();
    void startOverdubRegion();
    void stopOverdubRegion();
    void advanceRegion(int frames);
    
    bool inRecordingMode();

    //
    // Misc utilities
    //

    void resumePlay();
    const char* getModeName();
    const char* getModeName(MobiusMidiState::Mode mode);
    int simulateLevel(int count);
    void captureLevels(MobiusMidiState::Track* state);

};
