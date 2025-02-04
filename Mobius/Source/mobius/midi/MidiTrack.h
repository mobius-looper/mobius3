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
#include "../../model/TrackState.h"
#include "../../model/Session.h"
#include "../../model/ParameterConstants.h"

#include "../Notification.h"

#include "../track/BaseTrack.h"
#include "../track/LooperTrack.h"
#include "../track/LooperScheduler.h"
#include "../track/MslTrack.h"
#include "../track/TrackProperties.h"

#include "MidiRecorder.h"
#include "MidiPlayer.h"

class MidiTrack : public LooperTrack, public MslTrack
{
    friend class MidiPlayer;
    friend class MidiRecorder;
    
  public:

    //
    // Configuration
    //

    MidiTrack(class TrackManager* tm, class LogicalTrack* lt);
    ~MidiTrack();

    //
    // BaseTrack
    //
    
    void loadSession(class Session::Track* def) override;
    void doAction(class UIAction* a) override;
    bool doQuery(class Query* q) override;
    void processAudioStream(class MobiusAudioStream* stream) override;
    void midiEvent(class MidiEvent* e) override;
    void getTrackProperties(class TrackProperties& props) override;
    void trackNotification(NotificationId notification, TrackProperties& props) override;
    int getGroup() override;
    bool isFocused() override;
    void refreshState(class TrackState* stsate) override;
    void refreshPriorityState(class PriorityState* state) override;
    void refreshFocusedState(class FocusedTrackState* state) override;
    void dump(class StructureDumper& d) override;
    class MslTrack* getMslTrack() override;
    void syncPulse(class Pulse* p) override;
    int getSyncLength() override;
    
    //
    // ScheduledTrack
    //

    int getFrames() override;
    int getFrame() override;
    TrackState::Mode getMode() override;
    bool isExtending() override;
    bool isPaused() override;
    float getRate() override;
    void doActionNow(class UIAction* a) override;
    void advance(int frames) override;
    void reset() override;
    void loop() override;
    void leaderReset(class TrackProperties& props) override;
    void leaderRecordStart() override;
    void leaderRecordEnd(class TrackProperties& props) override;
    void leaderMuteStart(class TrackProperties& props) override;
    void leaderMuteEnd(class TrackProperties& props) override;
    void leaderResized(class TrackProperties& props) override;
    void leaderMoved(class TrackProperties& props) override;

    //
    // LooperTrack
    //

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

    // also in ScheduledTrack
    //bool isPaused() override;
    void startPause() override;
    void finishPause() override;
   
    void doParameter(class UIAction* a) override;
    void doPartialReset() override;
    void doReset(bool full) override;
    void doStart() override;
    void doStop() override;
    void doPlay() override;
    void doUndo() override;
    void doRedo() override;
    void doInstantMultiply(int n) override;
    void doInstantDivide(int n) override;
    void doHalfspeed() override;
    void doDoublespeed() override;
    
    bool isNoReset() override;

    //
    // MslTrack, many of these are in other interfaces
    //
    
    int getSubcycleFrames() override;
    //int getCycleFrames() override;
    //int getFrames() override;
    //int getFrame() override;
    //float getRate() override;
    bool scheduleWaitFrame(class MslWait* w, int frame) override;
    bool scheduleWaitEvent(class MslWait* w) override;
    //int getLoopCount() override;
    //int getLoopIndex() override;
    //int getCycles() override;
    //int getSubcycles() override;
    //TrackState::Mode getMode() override;
    //bool isPaused() override;

    bool isMuted() override;
    bool isOverdub() override;

    //
    // Extensions outside BaseTrack
    //
    
    void loadLoop(MidiSequence* seq, int loop);

  protected:

    //
    // Player support
    //
    
    // Support for Player
    void midiSend(juce::MidiMessage& msg, int deviceId);

    // Support for Recorder
    class MidiEvent* getHeldNotes();
    
  private:

    class SyncMaster* syncMaster = nullptr;
    class MidiPools* pools = nullptr;

    LooperScheduler scheduler;

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
    
    juce::Array<TrackState::Region> regions;
    int activeRegion = -1;
    
    // state
    TrackState::Mode mode = TrackState::ModeReset;
    TrackState::Mode prePauseMode = TrackState::ModeReset;
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

    // unused after the track reorg, I think BaseScheduler releated to undo
    bool isRecording();

    // used to be called by a scheduler
    void clipStart(int audioTrack, int loopIndex);
    
    // advance support
    void advancePlayer(int newFrames);
    void shift(bool unrounded);
    
    // regions
    void resetRegions();
    void startRegion(TrackState::RegionType type);
    void stopRegion();
    void resumeOverdubRegion();
    void advanceRegion(int frames);

    // various function support
    bool inRecordingMode();
    void followerPauseRewind();
    void resumePlay();
    const char* getModeName();
    int simulateLevel(int count);
    void captureLevels(class TrackState* state);

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
