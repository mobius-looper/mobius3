
#ifndef SYNCHRONIZER_H
#define SYNCHRONIZER_H

// necessary for SyncSource
#include "../../model/Setup.h"

#include "SyncState.h"
 
/****************************************************************************
 *                                                                          *
 *   							  CONSTANTS                                 *
 *                                                                          *
 ****************************************************************************/

// OBSOLETE: Should be moved somewhere under SyncMaster


/**
 * The minimum tempo we allow when generating MIDI clocks.
 */
#define SYNC_MIN_TEMPO 10

/**
 * The maximum tempo we allow when generating MIDI clocks.
 */
#define SYNC_MAX_TEMPO 400

/****************************************************************************
 *                                                                          *
 *                               SYNC UNIT INFO                             *
 *                                                                          *
 ****************************************************************************/

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

/****************************************************************************
 *                                                                          *
 *   							 SYNCHRONIZER                               *
 *                                                                          *
 ****************************************************************************/

class Synchronizer {

  public:

	Synchronizer(class Mobius* mob);
	~Synchronizer();

	void updateConfiguration(class MobiusConfig* config);
    void globalReset();
    
    //
    // Misc status
    //

    void getState(class OldMobiusState* state);
	void getState(class OldMobiusTrackState* state, class Track* t);

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

    void loopRealignSlave(Loop* l);
    void loopLocalStartPoint(Loop* l);
    void loopReset(Loop* loop);
    void loopRecordStart(Loop* l);
    void loopRecordStop(Loop* l, Event* stop);
    void loopResize(Loop* l, bool resize);
    void loopSpeedShift(Loop* l);
    void loopSwitch(Loop* l, bool resize);
    void loopPause(Loop* l);
    void loopResume(Loop* l);
    void loopMute(Loop* l);
    void loopRestart(Loop* l);
    void loopMidiStart(Loop* l);
    void loopMidiStop(Loop* l, bool force);
    void loopSetStartPoint(Loop* l, Event* e);
    void loopDrift(Loop* l, int delta);

    //
    // Sync Masters
    //

    class Track* getTrackSyncMaster();
    class Track* getOutSyncMaster();
    void setTrackSyncMaster(class Track* master);
    void setOutSyncMaster(class Track* master);
    void loadProject(class Project* p);
    void loadLoop(class Loop* l);

    //
    // Pulse processing
    //

    void syncEvent(class Loop* l, class Event* e);
    

	/////////////////////////////////////////////////////////////////////
	// 
	// Private
	//
	/////////////////////////////////////////////////////////////////////
    
  private:

    // record scheduling
    
    bool isRecordStartSynchronized(class Loop* l);
    bool isThresholdRecording(class Loop* l);
    class Event* schedulePendingRecord(class Action* action, class Loop* l, class MobiusMode* mode);
    
    bool isRecordStopPulsed(class Loop* l);
    void getAutoRecordUnits(class Loop* loop, float* retFrames, int* retBars);
    void setAutoStopEvent(class Action* action, class Loop* loop, class Event* stop, 
                          float barFrames, int bars);
    class Event* scheduleSyncRecordStop(class Action* action, class Loop* l);
    void getRecordUnit(class Loop* l, class SyncUnitInfo* unit);
    float getSpeed(class Loop* l);
    void traceTempo(class Loop* l, const char* type, float tempo);
    float getFramesPerBeat(float tempo);
    void adjustBarUnit(class Loop* l, class SyncState* state, SyncSource src, 
                       class SyncUnitInfo *unit);

    int getBeatsPerBar(SyncSource src, Loop* l);

    // pulse processing

    // weed
    
    void checkPulseWait(Loop* l, Event* e);
    void syncPulseWaiting(Loop* l, Event* e);
    void startRecording(Loop* l, Event* e);

    void syncPulseRecording(Loop* l, Event* e);
    void checkRecordStop(Loop* l, Event* pulse, Event* stop);
    void activateRecordStop(Loop* l, Event* pulse, Event* stop);

    void syncPulsePlaying(Loop* l, Event* e);
    void doRealign(Loop* l, Event* pulse, Event* realign);
    void realignSlave(Loop* l, Event* pulse);
    void traceDealign(Loop* l);

    

    void checkDrift();
    void correctDrift();
    void checkDrift(class SyncTracker* tracker);
    void correctDrift(class SyncTracker* tracker);
    bool isDriftCorrectable(class Track* track, class SyncTracker* tracker);
    void correctDrift(class Track* track, class SyncTracker* tracker);
    long wrapFrame(class Loop* l, long frame);
    void moveLoopFrame(class Loop* l, long newFrame);

    bool isTrackReset(class Track* t);
    void unlockTrackers();
    void unlockTracker(class SyncTracker* tracker);
    void informFollowers(SyncTracker* tracker, Loop* loop);
    class SyncTracker* getSyncTracker(Loop* l);
    class SyncTracker* getSyncTracker(SyncSource src);
    void muteMidiStop(Loop* l);

    void lockOutSyncTracker(class Loop* l, bool recordStop);
    void setOutSyncMasterInternal(class Track* t);
    class Track* findTrackSyncMaster();
    class Track* findOutSyncMaster();
    void resizeOutSyncTracker();
    float calcTempo(Loop* l, int beatsPerBar, long frames, int* retPulses);
    void sendStart(Loop* l, bool checkManual, bool checkNear);

    // temporary porting to SyncMaster that doesn't take a TraceContext
    void fullStop(TraceContext* l, const char* msg);

    // new event conversion
    class Event* getEvents(class MidiQueue* queue, class EventPool* pool, long interruptFrames);

	/////////////////////////////////////////////////////////////////////
	// 
	// Fields
	//
	/////////////////////////////////////////////////////////////////////

	// our leader
	class Mobius* mMobius;

    // our eventual upstart replacement, the ass kissing bastard
    class SyncMaster* mSyncMaster = nullptr;
    
	// queue for external MIDI events
	MidiQueue mMidiQueue;

    // Trackers for the three sync sources
    class SyncTracker* mHostTracker;
    class SyncTracker* mMidiTracker;
    class SyncTracker* mOutTracker;

	// currently designated output sync master
	class Track* mOutSyncMaster;

	// currently designated track sync master
	class Track* mTrackSyncMaster;

	// cached global config
	int mMaxSyncDrift;
	DriftCheckPoint mDriftCheckPoint;
	MidiRecordMode mMidiRecordMode;
    bool mNoSyncBeatRounding;

	// state captured during each interrupt
    class EventList* mInterruptEvents;
    Event* mReturnEvent;
    Event* mNextAvailableEvent;

	float mHostTempo;
	int mHostBeat;
	int mHostBeatsPerBar;
    bool mHostTransport;
    bool mHostTransportPending;
	long mLastInterruptMsec;
	long mInterruptMsec;
	long mInterruptFrames;

    // flag that may be set by the DriftCorrect function
    // to force a drift correction on the next interrupt
    bool mForceDriftCorrect;

	// temporary debuging kludge
	bool mKludgeBreakpoint;
};

#endif
