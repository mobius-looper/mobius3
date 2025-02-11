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

    void renumber(int n);

    // the number of this track in the LogicalTrack/Session list
    // this is what needs to be used when communicating with the outside world.
    void setLogicalNumber(int n);
    int getLogicalNumber();

    void dump(class StructureDumper& d);

    int getInputPort() {
        return mInputPort;
    }
    
    void setInputPort(int p) {
        mInputPort = p;
    }

    int getOutputPort() {
        return mOutputPort;
    }

    void setOutputPort(int p) {
        mOutputPort = p;
    }

    // used for sync parameters rather than having
    // to maintain a local copy
    class SetupTrack* getSetup();
    
	void setHalting(bool b);

	void getTraceContext(int* context, long* time);

    //void doFunction(Action* action);
    
	// Parameters

    void setName(const char* name);
    char* getName();

    void setGroup(int i);
    int getGroup();

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

	void setFocusLock(bool b);
	bool isFocusLock();

    // Setup/Preset management

    // change the setup at runtime
    void changeSetup(class Setup* setup);

    // change to a different active preset at runtime
	void changePreset(int number);

    // refresh the local preset copy
    // this is mostly internal but InitScriptStatement needs access to it
	void refreshPreset(class Preset* p);

    // return the local Preset copy
	class Preset* getPreset();

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
	 * Called by Mobius at the start of an interrupt to assimilate 
     * configuration changes.
	 */
	void updateConfiguration(class MobiusConfig* config);

    /**
     * Called by Mobius when the block size changes.
     */
    void updateLatencies(int inputLatency, int outputLatency);

    /**
     * Special function called within the script interpreter to
     * assimilate just global parameter changes.
     */
	void updateGlobalParameters(class MobiusConfig* config);

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
    
    void enterCriticalSection(const char* reason);
    void leaveCriticalSection();
    class InputStream* getInputStream();
    class OutputStream* getOutputStream();

  private:

	void init(Mobius* mob, class Synchronizer* sync, int number);

    // configuration management
    
    void propagateSetup(MobiusConfig* config, Setup* setup, bool setupsEdited, bool presetsEdited);
    class Preset* getStartingPreset(MobiusConfig* config, Setup* setup, bool globalReset);
    int getGroupNumber(class MobiusConfig* config, class SetupTrack* st);
	void setupLoops();
	void resetParameters(class Setup* setup, bool global, bool doPreset);
	void resetPorts(class SetupTrack* st);
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

	int mRawNumber;        // zero based
    int mLogicalNumber;    // the number used in TrackManager/SyncMaster/MobiusView
    char mName[MAX_TRACK_NAME];

	class Mobius* mMobius;
    class Notifier* mNotifier = nullptr;
	class Synchronizer* mSynchronizer;
    class SetupTrack* mSetupCache = nullptr;
    int mSetupOrdinal = -1;
    class EventManager* mEventManager;
	class InputStream* mInput;
	class OutputStream* mOutput;
	//class CriticalSection* mCsect;
	class UserVariables* mVariables;
    
    // private copy of the active preset
    // !! needs to be a member object
	class Preset* mPreset;

	class Loop*	mLoops[MAX_LOOPS];
    class Loop* mLoop;
	int			mLoopCount;

    int         mInputPort;
    int         mOutputPort;
    int         mGroup;
	bool 		mFocusLock;
	bool		mHalting;
	bool		mRunning;
    int         mMonitorLevel;
	bool		mGlobalMute;
	bool 		mSolo;
    bool        mThroughMonitor = false;
	int         mInputLevel;
	int         mOutputLevel;
	int         mFeedbackLevel;
	int         mAltFeedbackLevel;
	int         mPan;
    int         mSpeedToggle;
	bool        mMono;
	bool        mUISignal;
	int         mSpeedSequenceIndex;
	int         mPitchSequenceIndex;

    /**
     * Support for an old feature where we could move the controls
     * for a group (only output level) keeping the same relative
     * mix.  This no longer works.
     */
	int         mGroupOutputBasis;

    // Track sync event encountered during the last interrupt
    Event* mTrackSyncEvent;

	// debug/test state

    bool mInterruptBreakpoint;

	// state exposed to the outside world
    // we no longer maintain an internal copy, we will be given
    // the one to fill in in the getState() call
	//MobiusTrackState mState;

    // true if this is a MIDI track
    bool mMidi;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
