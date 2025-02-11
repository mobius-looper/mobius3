/**
 * Largely gutted during the Great SyncMaster Reorganization
 */

#pragma once

// necessary for SyncSource
#include "../../model/ParameterConstants.h"

// necessary for SyncMaster::RequestResult
#include "../sync/SyncMaster.h"

class Synchronizer {

  public:

	Synchronizer(class Mobius* mob);
	~Synchronizer();

    // needed by a few internals used to getting things from Synchronizer
    class SyncMaster* getSyncMaster() {
        return mSyncMaster;
    }

    void globalReset();
    
    //
    // Sync Pulses
    //

    void syncEvent(class Track* t, class SyncEvent* e);

    //
    // Record scheduling
    //

    class Event* scheduleRecordStart(class Action* action, class Function* function, class Loop* l);
    class Event* scheduleRecordStop(class Action* action, class Loop* loop);
    
    void undoRecordStop(class Loop* loop);
    void redoRecordStop(class Loop* loop);

    // 
    // Loop and Function notifications
    //

    void trackSyncEvent(class Track* t, class EventType* type, int offset);
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

	class Mobius* mMobius;
    class SyncMaster* mSyncMaster = nullptr;

    // Recording
    
    class Event* scheduleAutoRecordStart(class Action* action, class Function* function, class Loop* l);
    class Event* scheduleAutoRecordStop(class Action* action, class Loop* loop,
                                        SyncMaster::RequestResult& result);
    class Event* schedulePreRecordStop(class Action* action, class Loop* loop);
    
    class Event* scheduleSyncRecord(class Action* action, class Loop* l, class MobiusMode* mode);
    class Event* scheduleRecordStartNow(class Action* action, class Function* f, class Loop* l);
    class Event* scheduleNormalRecordStop(class Action* action, class Loop* loop);
    class Event* scheduleSyncRecordStop(class Action* action, class Loop* l);

    void reduceRecordStop(class Loop* loop);
    void extendRecordStop(class Loop* loop);
    float getSpeed(class Loop* l);

    // SyncEvent handling
    void startRecording(class Track* t, class SyncEvent* e);
    void stopRecording(class Track* t, class SyncEvent* e);
    void extendRecording(class Track* t, class SyncEvent* e);
    void finalizeRecording(class Track* t, class SyncEvent* e);
    void activateRecordStop(class Loop* l, class Event* stop);
    
    // Realign

    void doRealign(class Loop* loop, class Event* pulse, class Event* realign);
    void moveLoopFrame(class Loop* l, long newFrame);
    void realignSlave(class Loop* l, class Event* pulse);
    long wrapFrame(class Loop* l, long frame);

};
