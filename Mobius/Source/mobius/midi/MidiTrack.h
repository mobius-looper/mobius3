/**
 * A MidiTrack is a BaseTrack so it can live inside a LogicalTrack.
 * It makes use of the BaseScheduler and LooperScheduler to provide
 * most of the compolex action and event processing.
 */

#pragma once

#include "../../util/Util.h"
#include "../../model/MobiusState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"

#include "../../sync/Pulse.h"
#include "../Notification.h"

#include "../track/BaseTrack.h"
#include "../track/LooperTrack.h"
#include "../track/TrackProperties.h"

#include "MidiRecorder.h"
#include "MidiPlayer.h"

class MidiTrack : public LooperTrack
{
  public:

    //
    // Configuration
    //

    MidiTrack(class TrackManager* tm, class LogicalTrack* lt);
    ~MidiTrack();

    //
    // BaseTrack
    //
    
    virtual void loadSssion(class Session::Track* def) override;
    virtual void doAction(class UIAction* a) override;
    virtual bool doQuery(class Query* q) override;
    virtual void processAudioStream(class MobiusAudioStream* stream) override;
    virtual void midiEvent(class MidiEvent* e) override;
    virtual int getFrames() override;
    virtual int getFrame() override;
    virtual void getTrackProperties(class TrackProperties& props) override;
    virtual void trackNotification(NotificationId notification, TrackProperties& props) override;
    virtual int getGroup() override;
    virtual bool isFocused() override;
    virtual bool scheduleWaitFrame(class MslWait* w, int frame) override;
    virtual bool scheduleWaitEvent(class MslWait* w) override;
    virtual void refreshPriorityState(class MobiusState::Track* tstate) override;
    virtual void refreshState(class MobiusState::Track* tstate) override;

    //
    // ScheduledTrack
    //
    

    //
    // LooperTrack
    //

    virtual MobiusState::Mode getMode() override;
    virtual int getLoopCount() override;
    virtual int getLoopIndex() override;
    virtual int getCycleFrames() override;
    virtual int getCycles() override;
    virtual int getSubcycles() override;
    virtual int getModeStartFrame() override;
    virtual int getModeEndFrame() override;
    virtual int extendRounding() override;
    

    virtual void startRecord() override;
    virtual void finishRecord() override;

    virtual void startMultiply() override;
    virtual void finishMultiply() override;
    virtual void unroundedMultiply() override;
    
    virtual void startInsert() override;
    virtual int extendInsert() override;
    virtual void finishInsert() override;
    virtual void unroundedInsert() override;

    virtual void toggleOverdub() override;
    virtual void toggleMute() override;
    virtual void toggleReplace() override;
    virtual void toggleFocusLock() override;

    virtual void finishSwitch(int target) override;
    virtual void loopCopy(int previous, bool sound) override;

    virtual bool isPaused() override;
    virtual void startPause() override;
    virtual void finishPause() override;
   
    // simple one-shot actions
    virtual void doParameter(class UIAction* a) override;
    virtual void doPartialReset() override;
    virtual void doReset(bool full) override;
    virtual void doStart() override;
    virtual void doStop() override;
    virtual void doPlay() override;
    virtual void doUndo() override;
    virtual void doRedo() override;
    virtual void doDump() override;
    virtual void doInstantMultiply(int n) override;
    virtual void doInstantDivide(int n) override;
    virtual void doHalfspeed() override;
    virtual void doDoublespeed() override;
    
    // leader stuff
    virtual void leaderReset(class TrackProperties& props) override;
    virtual void leaderRecordStart() override;
    virtual void leaderRecordEnd(class TrackProperties& props) override;
    virtual void leaderMuteStart(class TrackProperties& props) override;
    virtual void leaderMuteEnd(class TrackProperties& props) override;
    virtual void leaderResized(class TrackProperties& props) override;
    virtual void leaderMoved(class TrackProperties& props) override;
    
    // advance play/record state between events
    virtual bool isExtending() override;
    virtual void advance(int newFrames) override;
    virtual void loop() override;

    virtual float getRate() override;
    virtual int getGoalFrames() override;
    virtual void setGoalFrames(int f) override;

    virtual bool isNoReset() override;

    //
    // Extensions outside BaseTrack
    //
    
    void loadLoop(MidiSequence* seq, int loop);

  private:

    LogicalTrack* logicalTrack = nullptr;
    LooperScheduler scheduler;
    
    void reset();

    //
    // Follower state
    //

    //
    // State
    //
    
    bool isRecording();

    //
    // stimuli
    //
    
    void processAudioStream(class MobiusAudioStream* argStream);

    void noteOn(class MidiEvent* e);
    void noteOff(class MidiEvent* e);
    void midiEvent(class MidiEvent* e);

    void clipStart(int audioTrack, int loopIndex);

    // Support for Recorder
    class MidiEvent* getHeldNotes();
    class MidiEvent* copyNote(class MidiEvent* src);

    // Support for Player
    void midiSend(juce::MidiMessage& msg, int deviceId);

    // leader support
    void leaderResize();
    void leaderRelocate();
    void leaderReorient();

  private:

    class TrackManager* manager = nullptr;
    class LogicalTrack* logicalTrack = nullptr;
    class Valuator* valuator = nullptr;
    class Pulsator* pulsator = nullptr;
    class MidiPools* pools = nullptr;

    // the number is only used for logging messages
    // and correlation of events
    int number = 0;

    // leader state
    bool followerMuteStart = false;
    bool followLocation = false;
    bool noReset = false;
    int outputChannel = 0;
    
    // loops
    juce::OwnedArray<class MidiLoop> loops;
    int loopCount = 0;
    int loopIndex = 0;
    bool loopsLoaded = false;

    // the meat
    MidiRecorder recorder {this};
    MidiPlayer player {this};
    TrackScheduler scheduler {this};
    ActionTransformer transformer {this, &scheduler};
    
    juce::Array<MobiusState::Region> regions;
    int activeRegion = -1;
    
    // state
    MobiusState::Mode mode = MobiusState::ModeReset;
    MobiusState::Mode prePauseMode = MobiusState::ModeReset;
    bool overdub = false;
    bool mute = false;
    bool reverse = false;
    int group = 0;
    bool focus = false;
    //bool pause = false;
    
    int input = 127;
    int output = 127;
    int feedback = 127;
    int pan = 64;
    int subcycles = 4;
    int inputMonitor = 0;
    int inputDecay = 0;
    int outputMonitor = 0;
    int outputDecay = 0;
    bool midiThru = false;
    
    // rate shift/resize
    float rate = 0.0f;
    int goalFrames = 0;
    
    // advance
    void advancePlayer(int newFrames);
    void shift(bool unrounded);
    
    //
    // Function Handlers
    //
    
    void resetRegions();
    void startRegion(MobiusState::RegionType type);
    void stopRegion();
    void resumeOverdubRegion();
    void advanceRegion(int frames);
    
    bool inRecordingMode();

    //
    // Leader/Follower handlers
    //

    void followerPauseRewind();
    int leaderLocate();

    //
    // Misc utilities
    //

    void resumePlay();
    const char* getModeName();
    const char* getModeName(MobiusState::Mode mode);
    int simulateLevel(int count);
    void captureLevels(MobiusState::Track* state);
    void resize();

    //
    // Ugly Math
    //
    
    float followLeaderLength(int myFrames, int otherFrames);
    int followLeaderLocation(int myFrames, int myLocation, int otherFrames, int otherLocation,
                             float rate, bool igoreCurrent, bool favorLate);

    void followLeaderSize();
    void followLeaderLocation();
    void reorientFollower(int previousFrames, int previoiusFrame);

};
