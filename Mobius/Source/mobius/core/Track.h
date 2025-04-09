/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An extension of RecorderTrack that adds Mobius functionality.
 *
 */

#ifndef TRACK_H
#define TRACK_H

#include "../../util/Trace.h"

#include "../Notification.h"

/****************************************************************************
 *                                                                          *
 *                                   TRACK                                  *
 *                                                                          *
 ****************************************************************************/

#define MAX_PENDING_ACTIONS 10

/**
 * The maximum number of loops in a track.
 * This needs to be fixed and relatively small so we can preallocate
 * the maximum number of Loop objects and simply enable or disable
 * them based on the Preset::loopCount parameter.  This saves memory churn
 * and ensures that we won't delete an object out from under a thread that
 * may still be referencing it, mostly this is the UI refresh thread.
 *
 * Prior to 2.0 this awa 128 which is insanely large.  16 is about the most
 * that is manageable and even then the UI for the loop list is practically
 * useless.  Still we could have this as a hidden global parameter.
 */
#define MAX_LOOPS 16

/**
 * Maximum name we can assign to a track.
 */
#define MAX_TRACK_NAME 128


class Track : public TraceContext
{

    friend class Mobius;
	friend class ScriptInterpreter;
	friend class ScriptSetupStatement;
    friend class Loop;
	friend class Synchronizer;
	friend class Function;
	friend class RecordFunction;
    friend class EventManager;
    friend class StreamState;

  public:

	Track(class Mobius* mob, class Synchronizer* sync, int number);
	~Track();

    //////////////////////////////////////////////////////////////////////
    // New Interface
    //////////////////////////////////////////////////////////////////////

    //
    // Start making these look like BaseTracks
    
    void renumber(int n);
    void doAction(class UIAction* a);
    void refreshParameters();

    class LogicalTrack* getLogicalTrack();
    void setLogicalTrack(class LogicalTrack* lt);
    int getLogicalNumber();

    void dump(class StructureDumper& d);

    void gatherContent(class TrackContent* c);

    //////////////////////////////////////////////////////////////////////
    // Old Interface
    //////////////////////////////////////////////////////////////////////

	void setHalting(bool b);

	void getTraceContext(int* context, long* time);

    //void doFunction(Action* action);
    
	// Parameters

	void setInputLevel(int i);
	int  getInputLevel();

	void setOutputLevel(int i);
	int  getOutputLevel();

    void setFeedback(int i);
	int  getFeedback();

    void setAltFeedback(int i);
	int  getAltFeedback();

	void setPan(int i);
	int  getPan();

	void setMono(bool b);
	bool isMono();

    void setMidi(bool b);
    bool isMidi();

    void setSpeedToggle(int degree);
    int getSpeedToggle();

    // these have to be set with Functions and Events
    int getSpeedOctave();
    int getSpeedStep();
    int getSpeedBend();
    int getTimeStretch();
    int getPitchOctave();
    int getPitchStep();
    int getPitchBend();

	void setPitchTweak(int tweak, int value);
    int getPitchTweak(int tweak);

	void setGroupOutputBasis(int i);
	int getGroupOutputBasis();

	//void setFocusLock(bool b);
	//bool isFocusLock();

	//
    // status 
	//

	Mobius* getMobius();
	class Synchronizer* getSynchronizer();
    class EventManager* getEventManager();

	int getLoopCount();
	class Loop* getLoop(int index);
	int getRawNumber();
	int getDisplayNumber();
	bool isEmpty();

    // seem to have gotten lost
	//int getInputLatency();
	//int getOutputLatency();
    
	MobiusMode* getMode();
    int getFrames();
    int getCycles();
	long getFrame();
    void refreshState(class TrackState* s);
    void refreshPriorityState(class PriorityState* s);
    void refreshFocusedState(class FocusedTrackState* state);
    
	int getCurrentLevel();
	//bool isTrackSyncMaster();

	float getEffectiveSpeed();
	float getEffectivePitch();

	long getProcessedFrames();
	long getRemainingFrames();

	//
	// Mobius control
	//

	void loadProject(class ProjectTrack* t);
	void setBounceRecording(class Audio* a, int cycles);
	void setMuteKludge(class Function* f, bool b);

	/**
	 * Called by Mobius as scripts terminate.
	 */
	void removeScriptReferences(class ScriptInterpreter* si);

	//	
	// Function handlers
	//

	void reset(class Action* action);
	void loopReset(class Action* action, class Loop* loop);

	// sequence step for the SpeedNext function
	int getSpeedSequenceIndex();
	void setSpeedSequenceIndex(int s);

	// sequence step for the PitchNext function
	int getPitchSequenceIndex();
	void setPitchSequenceIndex(int s);

	//
	// Audio interrupt handler
	//

    // true if this is the Track Sync Master track
    // which will then be processed first
    bool isPriority();

    // !! if all this does is prepare for containerAudioAvailable
    // then we could merge them unless there are some subtle
    // order dependences with Action scheduling
	void prepareForInterrupt();

    void processAudioStream(class MobiusAudioStream* stream);
    void syncEvent(class SyncEvent* e);
    int getSyncLength();
    int getSyncLocation();
    
	//
    // Unit test interface
	//

    Audio* getPlaybackAudio();
	class Loop* getLoop();
    void setInterruptBreakpoint(bool b);
    void interruptBreakpoint();
	int getProcessedOutputFrames();

    class UserVariables* getVariables();

	void setGlobalMute(bool b);
	bool isGlobalMute();
	bool isMute();
	void setSolo(bool b);
	bool isSolo();

    //void dump(TraceBuffer* b);

    void notifyModeStart(class MobiusMode* mode);
    void notifyModeEnd(class MobiusMode* mode);
    void notifyLoopStart();
    void notifyLoopCycle();
    void notifyLoopSubcycle();
    
  protected:

    void setLoop(class Loop* l);
	void addTail(float* tail, long frames);

	void setUISignal();
	bool isUISignal();

    //
    // EventManager
    //
    
    class InputStream* getInputStream();
    class OutputStream* getOutputStream();

  private:

	void init(Mobius* mob, class Synchronizer* sync, int number);

    // configuration management

    void setInputPort(int p);
    void setOutputPort(int p);
	void setupLoops();
	void trackReset(class Action* action);
    bool checkSyncEvent(class Event* e);
    void switchLoop(class Function* f, bool forward);
    void switchLoop(class Function* f, int index);
	void checkFrames(float* buffer, long frames);
	void playTail(float* outbuf, long frames);
	float* playTailRegion(float* outbuf, long frames);

    void processBuffers(class MobiusAudioStream* stream, 
                        float* inbuf, float *outbuf, long frames);

    void notifyBufferModified(float* buffer);
    
	void advanceControllers();

    //
    // Fields
    //

	int mRawNumber = 0;        // zero based
    class LogicalTrack* mLogicalTrack = nullptr;

	class Mobius* mMobius = nullptr;
    class Notifier* mNotifier = nullptr;
	class Synchronizer* mSynchronizer = nullptr;
    class EventManager* mEventManager = nullptr;
	class InputStream* mInput = nullptr;
	class OutputStream* mOutput = nullptr;
	//class CriticalSection* mCsect;
	class UserVariables* mVariables = nullptr;
    
	class Loop*	mLoops[MAX_LOOPS];
    class Loop* mLoop = nullptr;
	int			mLoopCount = 0;

    int         mInputPort = 0;
    int         mOutputPort = 0;
	bool		mHalting = false;
	bool		mRunning = false;
    int         mMonitorLevel = 0;
	bool		mGlobalMute = false;
	bool 		mSolo = false;
    bool        mThroughMonitor = false;
	int         mInputLevel = 0;
	int         mOutputLevel = 0;
	int         mFeedbackLevel = 0;
	int         mAltFeedbackLevel = 0;
	int         mPan = 0;
    int         mSpeedToggle = 0;
	bool        mMono = false;
	bool        mUISignal = false;
	int         mSpeedSequenceIndex = 0;
	int         mPitchSequenceIndex = 0;

    /**
     * Support for an old feature where we could move the controls
     * for a group (only output level) keeping the same relative
     * mix.  This no longer works.
     */
	int         mGroupOutputBasis = 0;

    // Track sync event encountered during the last interrupt
    Event* mTrackSyncEvent = nullptr;

	// debug/test state

    bool mInterruptBreakpoint = false;

	// state exposed to the outside world
    // we no longer maintain an internal copy, we will be given
    // the one to fill in in the getState() call
	//MobiusTrackState mState;

    // true if this is a MIDI track
    bool mMidi = false;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
