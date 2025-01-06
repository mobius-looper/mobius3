/**
 * Largely gutted during the Great SyncMaster Reorganization
 */

#pragma once

// necessary for SyncSource
#include "../../model/ParameterConstants.h"
 
//////////////////////////////////////////////////////////////////////
//
// SyncUnitInfo
//
//////////////////////////////////////////////////////////////////////

/**
 * Little structure used in the calculation of recording "units".
 * Necessary because there are several properties of a unit that
 * are all calculated using similar logic.
 */
typedef struct {

    /**
     * Number of frames in the unit. For SYNC_MIDI this will be the
     * frames in a beat or bar calculated from the MIDI tempo being
     * monitored.  For SYNC_HOST this will be the number of frames
     * in a beat or bar measured between host events.  For SYNC_TRACK
     * this will be the number of frames in a master track subcycle,
     * cycle, or loop.
     *
     * For SYNC_MIDI this will be calculated from the measured tempo
     * and may be fractional.  It will later be truncated but we keep
     * it as a fraction now so if we need to multiply it to get a bar
     * length we avoid roundoff error.
     */
    float frames;

    /**
     * The number of sync pulses in the unit.  For SYNC_MIDI this
     * will be the number of clocks in the unit, beats times 24.
     * For SYNC_HOST this will be the number of host beats.  For
     * SYNC_TRACK this will be the number of master track subcycles.
     */
    int pulses;

    /**
     * The number of cycles in the unit.  For SYNC_TRACK the cycle width
     * comes from the master track so the result may be fractional.
     * For SYNC_HOST and SYNC_MIDI, each bar is considered to be one cycle, 
     * this is determined by the BeatsPerbar sync parameter.
     */
    float cycles;

    /**
     * The rate adjust frames in one unit.
     * This is unitFrames times the current amount of rate shift, 
     * with possible rounding to make it a multiple of the sync tracker.
     */
    float adjustedFrames;

} SyncUnitInfo;

//////////////////////////////////////////////////////////////////////
//
// Synchronizer
//
//////////////////////////////////////////////////////////////////////

class Synchronizer {

  public:

	Synchronizer(class Mobius* mob);
	~Synchronizer();

    // needed by a few internals used to getting things from Synchronizer
    class SyncMaster* getSyncMaster() {
        return mSyncMaster;
    }

	void updateConfiguration(class MobiusConfig* config);
    void globalReset();
    
    //
    // Misc status
    //

    void getState(class OldMobiusState* state);
	void getState(class OldMobiusTrackState* state, class Track* t);
    void refreshState(class TrackState* state, class Track* t);
    
    //
    // Sync Pulses
    //

    void syncPulse(class Track* t, class Pulse* p);

    //
    // Record scheduling
    //

    class Event* scheduleRecordStart(class Action* action, class Function* function, class Loop* l);
    class Event* scheduleRecordStop(class Action* action, class Loop* loop);
    void extendRecordStop(class Action* action, class Loop* loop, class Event* stop);
    bool undoRecordStop(class Loop* loop);

    //
    // Interrupt Lifecycle
    //

	void interruptStart(class MobiusAudioStream* stream);
    void prepare(class Track* t);
    void finish(class Track* t);
	void interruptEnd();

    void trackSyncEvent(class Track* t, class EventType* type, int offset);

    // 
    // Loop and Function callbacks
    //

    void loopRealignSlave(class Loop* l);
    void loopLocalStartPoint(class Loop* l);
    void loopReset(class Loop* loop);
    void loopRecordStart(class Loop* l);
    void loopRecordStop(class Loop* l, class Event* stop);
    void loopResize(class Loop* l, bool resize);
    void loopSpeedShift(class Loop* l);
    void loopSwitch(class Loop* l, bool resize);
    void loopPause(class Loop* l);
    void loopResume(class Loop* l);
    void loopMute(class Loop* l);
    void loopRestart(class Loop* l);
    void loopMidiStart(class Loop* l);
    void loopMidiStop(class Loop* l, bool force);
    void loopSetStartPoint(class Loop* l, class Event* e);

    void loadProject(class Project* p);
    void loadLoop(class Loop* l);

    // Temporary master interface for old functions
    class Track* getTrackSyncMaster();
    class Track* getOutSyncMaster();
    
	/////////////////////////////////////////////////////////////////////
	// 
	// Private
	//
	/////////////////////////////////////////////////////////////////////
    
  private:

	// our leader
	class Mobius* mMobius;

    // our eventual upstart replacement, the ass kissing bastard
    class SyncMaster* mSyncMaster = nullptr;

    // used by getFramesPerBeat, not sure where this came from
    bool mNoSyncBeatRounding = false;
    
    // record scheduling
    
    bool isRecordStartSynchronized(class Loop* l);
    bool isThresholdRecording(class Loop* l);
    class Event* schedulePendingRecord(class Action* action, class Loop* l, class MobiusMode* mode);
    
    bool isRecordStopPulsed(class Loop* l);
    void getAutoRecordUnits(class Loop* loop, float* retFrames, int* retBars);
    void setAutoStopEvent(class Action* action, class Loop* loop, class Event* stop, 
                          float barFrames, int bars);
    class Event* scheduleSyncRecordStop(class Action* action, class Loop* l);
    void getRecordUnit(class Loop* l, SyncUnitInfo* unit);
    float getSpeed(class Loop* l);
    void traceTempo(class Loop* l, const char* type, float tempo);
    float getFramesPerBeat(float tempo);
    int getBeatsPerBar(SyncSource src, Loop* l);


    void startRecording(class Loop* l);
    void syncPulseRecording(class Loop* l, class Pulse* p);
    void activateRecordStop(class Loop* l, class Pulse* pulse, class Event* stop);
    
    bool isTransportMaster(class Loop* l);

    void muteMidiStop(class Loop* l);

    void sendStart(class Loop* l, bool checkManual, bool checkNear);

    void doRealign(class Loop* loop, class Event* pulse, class Event* realign);
    void moveLoopFrame(class Loop* l, long newFrame);
    void realignSlave(class Loop* l, class Event* pulse);
    long wrapFrame(class Loop* l, long frame);


};
