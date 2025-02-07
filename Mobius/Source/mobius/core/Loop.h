/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 *
 * Model for a loop.
 *
 */

#ifndef LOOP_H
#define LOOP_H

#include "../../util/Trace.h"

#include "../../model/Preset.h"

//////////////////////////////////////////////////////////////////////
//
// Stream State
//
//////////////////////////////////////////////////////////////////////

/**
 * A little structure used to capture interesting loop state.
 * Used in JumpContext to hold the pending output stream state,
 * Used in Loop to hold previous stream state when loop transfer
 * mode is Restore.
 */
class StreamState {
  public:

    StreamState();
    ~StreamState();
    void init();
    void capture(class Track* t);

    long frame;
	bool reverse;
    int speedToggle;
    int speedOctave;
	int speedStep;
	int speedBend;
    int pitchOctave;
	int pitchStep;
	int pitchBend;
    int timeStretch;
};

/****************************************************************************
 *                                                                          *
 *                               EVENT CONTEXTS                             *
 *                                                                          *
 ****************************************************************************/

// These don't really belong here but I don't want them in Function.h
// since that's accessible to the UI.

/**
 * Helper struct to keep track of all the things we may need to change
 * during a play jump.  
 */
class JumpContext {
  public:
						   
	JumpContext() {}
	~JumpContext() {}

	// we make two passes one to get events that will change the latency
	// (speed events) and another after we adjust latency
	bool speedOnly;

	class Layer* layer;
	long frame;
	bool latencyLossOverride;
	bool mute;			// must be true to mute
	bool unmute;		// must be true to unmute
	bool muteForced;
	bool reverse;
	int speedToggle;
    int speedOctave;
	int speedStep;
	int speedBend;
    int timeStretch;
    bool speedRestore;
    int pitchOctave;
	int pitchStep;
	int pitchBend;
    bool pitchRestore;

	int inputLatency;
	int outputLatency;

};

/**
 * Helper struct to keep track of things that happen during a loop switch.
 */
class SwitchContext {
  public:

    SwitchContext() {}
    ~SwitchContext() {}

	bool loopCopy;
	bool timeCopy;
	bool singleCycle;
	bool record;
	bool mute;
    bool unmute;

};

/****************************************************************************
 *                                                                          *
 *                                    LOOP                                  *
 *                                                                          *
 ****************************************************************************/

class Loop : public TraceContext {

	friend class Mobius;
	friend class Function;
	friend class RecordFunction;
	friend class Synchronizer;
    friend class SyncRealignsVariableType;

  public:

	Loop(int number, class Mobius* mob, class Track* track, 
		 class InputStream* input, class OutputStream* output);
	~Loop();

    void dump(class StructureDumper& d);

    void kludgeSavePlayLayer();

	void getTraceContext(int* context, long* time);

	void updateConfiguration(class MobiusConfig* config);
	void loadProject(class ProjectLoop* l);

    // new individisual loop loader
    void loadLoopNew(Layer* layer, bool isActive);

	void setNumber(int i);
	int getNumber();
	void setBreak(bool b);

    bool isInteresting();
    //void dump(class TraceBuffer* b);

    //
    // Status
    //

    void refreshState(class TrackState* s);
    void refreshFocusedState(class FocusedTrackState* state);
    
    class StreamState* getRestoreState();
	class MobiusMode* getMode();
	bool isEmpty();
    class Function* isSyncWaiting();
	bool isReset();
	bool isHalfSpeed();
	bool isReverse();
	bool isOverdub();
	bool isRecording();
    bool isPlaying();
	bool isPaused();
	bool isAdvancing();
	bool isAdvancingNormally();
	bool isMute();
	bool isMuteMode();
	bool isSyncRecording();
	bool isSyncPlaying();
	int  getNextLoop();
    long getFrame();
    long getPlayFrame();
    long getFrames();
    long getHistoryFrames();
    long getRecordedFrames();
    long getModeStartFrame();
    long getCycles();
    void setCycles(int cycles);
    long getCycleFrames();
    long getSubCycleFrames();
    long getWindowOffset();
    int  getEffectiveFeedback();
    class Layer* getMuteLayer();
	class Layer* getRecordLayer();
	class Layer* getPlayLayer();
	class Layer* getRedoLayer();
    class Audio* getPlaybackAudio();
    void setAltFeedback(int i);
    int getAltFeedback();
	long getInputLatency();
	long getOutputLatency();
	int getSampleRate();
	class Mobius* getMobius();
	class Synchronizer* getSynchronizer();
    Track* getTrack();
	class Preset* getPreset();

    // Utilities

	long wrapFrame(long frame, long loopFrames);
	long wrapFrame(long frame);
	long addFrames(long frame, long frames);

	void validate(class Event* e);

	// Function/Event Handling for Track

	void record();
	void play();
	void setMuteKludge(class Function* f, bool b);

	//
	// Function handler implementations
	// Would like these to be protected, but then we'd have to have
    // a billion friends
	//

	void reset(class Action* action);
	void setPause(bool b);
	void setMuteMode(bool b);
	void setMute(bool b);
	void setMode(class MobiusMode* m);
	void setOverdub(bool b);
	void setRecording(bool b);

	InputStream* getInputStream();
	OutputStream* getOutputStream();

	void shift(bool checkAutoUndo);
	bool isLayerChanged(class Layer* l, bool checkAutoUndo);
	void setPlayLayer(Layer* l);
	void setPrePlayLayer(Layer* l);
	void setRecordLayer(Layer* l);
	void setRedoLayer(Layer* l);

    long getMinimumFrames();
	void prepareLoop(bool inputLatency, int extra);
	void finishRecording(class Event* e);
	void stopPlayback();
	void recordEndEventUndo(class Event* e);

	bool checkMuteCancel(class Event* e);
	void resumePlay();
	void cancelPrePlay();

	void validateEvent(class Event* e);
	void playEvent(class Event* e);

	Event* scheduleRoundingModeEnd(class Action* action, Event* event);
	void insertEvent(class Event* e);
	void cancelRehearse(class Event* event);
    Event* scheduleStutterTransition(bool ending);

	void shuffleEvent(class Event* e);
	Event* scheduleModeEndPlayJump(class Event* e, bool unrounded);
	void jumpPlayEvent(class Event* e);
	void jumpPlayEventUndo(class Event* e);
	void undoEvent(class Event* e);
	void addRedo(class Event* e, class Layer* l);
    void addUndo(class Layer* l);
	void redoEvent(class Event* e);
	void reversePlayEvent(class Event* e);
	void reversePlayEventUndo(class Event* e);
	void startPointEvent(class Event* event);
	void setReverse(bool b);

    void switchEvent(class Event* e);
    void returnEvent(class Event* e);

    void trackEvent(class Event* e);
    void trackCopyTiming(Track* src);
    void trackCopySound(Track* src);

	void loopEvent(class Event* e);

	void setBounceRecording(class Audio* a, int cycles);

    // Function event handlers
	long recalculateFrame(bool calcplay);
    void setFrame(long f);
    void setModeStartFrame(long f);
	void warpFrame();
	void recalculatePlayFrame();
	Event* scheduleReturnEvent(class Event* trigger, Loop* next, bool sustain);
    void stutterCycle();

    // needed by both MidiStartFunction and RealignFunction
	void cancelSyncMute(class Event* e);

    // MultiplyFunction
    bool isUnroundedEnding(class Function* f);

    // WindowFunction
    void movePlayFrame(long f);

  protected:


    void setPlayFrame(long f);
    void cancelFadeIn();
	void playLocal();

	// Synchronizer interface

	void setRecordCycles(int cycles);
    int getRecordCycles();
    
  private:

    // need to remove this
    // class MobiusLoopState* getState();

    void init(class Mobius* mob, class Track* track, 
			  class InputStream* input, class OutputStream* output);

	void clear();
    bool checkThreshold();
	void notifyBeatListeners(class Layer* layer, long frames);
	float* getPlayFrames(class Layer* layer, float* buffer, long frames);
	void checkBreak();
	long reverseFrame(long frame);

	void adjustJump(class Event* event, class JumpContext* next);
	void adjustSwitchJump(class Event* jump, class JumpContext* next);

	void moveModeEnd(class Event* endEvent, long newFrame);
	long getUnroundedRecordStopFrame(class Event* event);
	long getModeInsertedFrames(class Event* endEvent);
	long getModeEndFrame(class Event* event);

    long reflectFrame(long frame);

	bool undoRecordStop();

    Event* copySound(Loop* src, class Function* initial, bool checkMode, long modeFrame);
    Event* copyTiming(Loop* src, long modeFrame);
    void trackCopySound(Loop* src, Loop* dest);
    void trackCopyTiming(Loop* src, Loop* dest);
    void getTrackCopyFrame(Loop* src, Loop* dest, long* startFrame, long* modeFrame);
    void switchRecord(Loop* next, Event* switchEvent, Event* stackedEvent);

    //////////////////////////////////////////////////////////////////////
    //
    // Fields
    //
    //////////////////////////////////////////////////////////////////////

    class Mobius* mMobius;
	class Track* mTrack;
	class Preset* mPreset;		// copy of Track's Preset
	class InputStream* mInput;
	class OutputStream* mOutput;
	class Synchronizer* mSynchronizer;
    class Layer* mRecord;
    class Layer* mPlay;
    class Layer* mPrePlay;
	class Layer* mRedo;

	int mNumber;
    long mFrame;
	long mPlayFrame;
    long mModeStartFrame;
	MobiusMode* mMode;
	
	// The distinction between mMute and mMuteMode is subtle.
	// mMute is on whenever a mute is active, preventing output.
	// mMuteMode tracks the state of the "mute minor mode" that may
	// be toggled on and off without necessarily having mMode==MuteMode.
	// mMuteMode is therefore like mOverdub.  For consistency should
	// call this mOverdubMode?

	bool mMute;
	bool mPause;
	bool mMuteMode;
	bool mOverdub;
	bool mRecording;
	bool mAutoFeedbackReduction;
	bool mBreak;

	// saved state for TransferMode=Remember
    StreamState mRestoreState;

	bool 	mBeatLoop;
	bool	mBeatCycle;
	bool 	mBeatSubCycle;

	//MobiusLoopState mState;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif
