/**
 * A MidiTrack is a BaseTrack so it can live inside a LogicalTrack.
 *
 * It is also a LooperTrack and uses LooperSchedule to handle the complex
 * action scheduling and mode transitions.
 *
 * And it is an MslTrack so it can play MSL games.
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
#include "../track/MslTrack.h"
#include "../track/TrackProperties.h"

#include "MidiRecorder.h"
#include "MidiPlayer.h"

class MidiTrack : public LooperTrack, public MslTrack
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
    
    void loadSssion(class Session::Track* def) override;
    void doAction(class UIAction* a) override;
    bool doQuery(class Query* q) override;
    void processAudioStream(class MobiusAudioStream* stream) override;
    void midiEvent(class MidiEvent* e) override;
    int getFrames() override;
    int getFrame() override;
    void getTrackProperties(class TrackProperties& props) override;
    void trackNotification(NotificationId notification, TrackProperties& props) override;
    int getGroup() override;
    bool isFocused() override;
    bool scheduleWaitFrame(class MslWait* w, int frame) override;
    bool scheduleWaitEvent(class MslWait* w) override;
    void refreshPriorityState(class MobiusState::Track* tstate) override;
    void refreshState(class MobiusState::Track* tstate) override;
    void dump(class StructureDumper& d) override;
    class MslTrack* getMslTrack() override;
    
    //
    // ScheduledTrack
    //
    

    //
    // LooperTrack
    //

    MobiusState::Mode getMode() override;
    int getLoopCount() override;
    int getLoopIndex() override;
    int getCycleFrames() override;
    int getCycles() override;
    int getSubcycles() override;
    int getModeStartFrame() override;
    int getModeEndFrame() override;
    int extendRounding() override;
    

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
    void toggleFocusLock() override;

    void finishSwitch(int target) override;
    void loopCopy(int previous, bool sound) override;

    bool isPaused() override;
    void startPause() override;
    void finishPause() override;
   
    // simple one-shot actions
    void doParameter(class UIAction* a) override;
    void doPartialReset() override;
    void doReset(bool full) override;
    void doStart() override;
    void doStop() override;
    void doPlay() override;
    void doUndo() override;
    void doRedo() override;
    void doDump() override;
    void doInstantMultiply(int n) override;
    void doInstantDivide(int n) override;
    void doHalfspeed() override;
    void doDoublespeed() override;
    
    // leader stuff
    void leaderReset(class TrackProperties& props) override;
    void leaderRecordStart() override;
    void leaderRecordEnd(class TrackProperties& props) override;
    void leaderMuteStart(class TrackProperties& props) override;
    void leaderMuteEnd(class TrackProperties& props) override;
    void leaderResized(class TrackProperties& props) override;
    void leaderMoved(class TrackProperties& props) override;
    
    // advance play/record state between events
    bool isExtending() override;
    void advance(int newFrames) override;
    void loop() override;

    float getRate() override;
    int getGoalFrames() override;
    void setGoalFrames(int f) override;

    bool isNoReset() override;

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
