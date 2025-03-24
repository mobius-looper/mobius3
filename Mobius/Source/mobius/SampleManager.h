/**
 * manages a collection of audio fragments that may be injected into
 * the real-time audio stream.  This was developed for Mobius testing
 * but could provide the foundatation for a more flexible sample playback
 * plugin.
 *
 * One unusual feature is that samples can not only be played but they
 * can be injected into the INPUT buffers of the audio stream for processing
 * by other things.  This is how we feed audio data into Mobius without
 * having to actually be playing an instrument.
 *
 * The samples to manage are defined in a SampleConfig object edited by the UI
 * and stored in files.  These are read by the SampleLoader to construct
 * the SampleManager, SamplePlayer, and SampleCursors.
 *
 * Samples are triggered from the UI with a UIAction with the UIFunction
 * SampleTrigger
 *
 */

#pragma once

//#include <stdio.h>

#include "../model/SampleConfig.h"

//////////////////////////////////////////////////////////////////////
//
// SampleTrigger
//
//////////////////////////////////////////////////////////////////////

#define MAX_TRIGGERS 8

/**
 * Helper struct to represent one sample trigger event.
 * Each SamplePlayer maintains an array of these which are filled
 * by the ui and/or MIDI thread, and consumed by the audio thread.
 * To avoid a critical section, there are two indexes into the array.
 * The "head" index is the index of the first element that needs
 * to be processed by the audio thread.  The "tail" index is the
 * index of the next element available to be filled by the ui interrupt.
 * When the head and tail indexes are the same there is nothing in the
 * queue.  Only the audio thread advances the head index, only the ui
 * thread advances the tail index.  If the tail indexes advances to
 * the head index, there is an overflow.
 *
 * We don't handle trigger overflow gracefully, but this could only
 * happen if you were triggering more rapidly than audio interrupt
 * interval.  In practice, humans couldn't do this, though another
 * machine feeding us MIDI triggers could cause this.
 *
 * UPDATE: Sample triggering is now handled by the Action model
 * so triggers will always be done inside the interrupt, we don't
 * need the ring buffer.
 */
typedef struct {

    // true if this is a down transition
    bool down;

} SampleTrigger;

//////////////////////////////////////////////////////////////////////
//
// SamplePlayer
//
//////////////////////////////////////////////////////////////////////

/**
 * Represents one loaded sample that can be played by SampleManager.
 *
 * old comment:
 * Might be interesting to give this capabilities like Segemnt
 * or Layer so we could dynamically define samples from loop material.
 */
class SamplePlayer
{
    friend class SampleCursor;

  public:

	SamplePlayer(class AudioPool* pool, Sample* s);
	~SamplePlayer();

    // !! revisit this
    // supposed to respond to reconfiguration of the audio device
    // while in the kernel, doesn't happen yet
    void updateConfiguration(int inputLatency, int outputLatency);

	void setNext(SamplePlayer* sp);
	SamplePlayer* getNext();

    // filename saved only for different detection
    const char* getFilename();

	void setAudio(class Audio* a);
	class Audio* getAudio();
	long getFrames();

	void setSustain(bool b);
	bool isSustain();

	void setLoop(bool b);
	bool isLoop();

    void setConcurrent(bool b);
    bool isConcurrent();

	void trigger(bool down);
	void play(float* inbuf, float* outbuf, long frames);

    // hack for testing so Samples can get buttons like scripts
    void setButton(bool b) {
        mButton = b;
    }
    bool isButton() {
        return mButton;
    }
    
  protected:

    //
    // Configuration caches.
    // I don't really like having these here but I don't want to 
    // introduce a dependency on MobiusConfig or MobiusContainer at this level.
    // Although these are only used by SampleCursor, they're maintained here to 
    // make them easier to update.
    // Since they apply to anything within the MobiusContainer they should
    // be on the SampleManager, not each SamplePlayer
    //
    
	/**
	 * Number of frames to perform a gradual fade out when ending
	 * the playback early.  Supposed to be synchronized with
	 * the MobiusConfig, but could be independent.
	 */
	long mFadeFrames;

    /**
     * Number of frames of input latency, taken from the audio stream container
     */
    long mInputLatency;

    /**
     * Number of frames of output latency.
     */
    long mOutputLatency;

  private:
	
	void init();
    class SampleCursor* newCursor();
    void freeCursor(class SampleCursor* c);

	SamplePlayer* mNext;
	class Audio* mAudio;

	// flags copied from the Sample
    char* mFilename;
	bool mSustain;
	bool mLoop;
    bool mConcurrent;

    /**
     * A queue of trigger events, filled by the ui thread and
     * consumed by the audio thread.
     * jsl - comments say this is no longer used, really?
     */
    SampleTrigger mTriggers[MAX_TRIGGERS];

    int mTriggerHead;
    int mTriggerTail;

    /**
     * As the sample is triggered, we will active one or more 
     * SampleCursors.  This is the list of active cursors.
     */
    class SampleCursor* mCursors;

    /**
     * A pool of unused cursors.
     * These are still allocated dynamically which is bad, and needs
     * to be redesigned into a propery pool with allocations managed
     * by the shell.
     */
    class SampleCursor* mCursorPool;

    /**
     * Transient runtime trigger state to detect keyboard autorepeat.
     * This may conflict with MIDI triggering!?
     *
     * UPDATE: Key repeat is now suppressed at a much higher level, may
     * be able to get rid of this.
     */
    bool mDown;

    bool mButton = false;
    
};

//////////////////////////////////////////////////////////////////////
//
// SampleCursor
//
//////////////////////////////////////////////////////////////////////

/**
 * Encapsulates the state of one trigger of a SamplePlayer.
 * A SamplePlayer may activate more than one of these if the sample
 * is triggered again before the last one, and I assume if concurrency
 * is allowed.
 */
class SampleCursor
{
    friend class SamplePlayer;

  public:
    
    SampleCursor();
    SampleCursor(SamplePlayer* s);
    ~SampleCursor();

    SampleCursor* getNext();
    void setNext(SampleCursor* next);

    void play(float* inbuf, float* outbuf, long frames);
    void play(float* outbuf, long frames);

    void stop();
    bool isStopping();
    bool isStopped();

  protected:

    // for SamplePlayer
    void setSample(class SamplePlayer* s);

  private:

    void init();
	void stop(long maxFrames);

    SampleCursor* mNext;
    // this cursor is used when injecting audio into the input buffers??
    // forget how this worked
    SampleCursor* mRecord;
    SamplePlayer* mSample;
	class AudioCursor* mAudioCursor;

    bool mStop;
    bool mStopped;
    long mFrame;

	/**
	 * When non-zero, the number of frames to play, which may
     * be less than the number of available frames.
	 * This is used when a sustained sample is ended prematurely.  
	 * We set up a fade out and continue past the trigger frame 
	 * to this frame.  Note that this is a frame counter, 
     * not the offset to the last frame.  It will be one beyond
     * the last frame that is to be played.
	 */
	long mMaxFrames;

};

//////////////////////////////////////////////////////////////////////
//
// SampleManager
//
//////////////////////////////////////////////////////////////////////

/**
 * The maximum number of samples that SampleManager can manage.
 */
#define MAX_SAMPLES 8

/**
 * Makes a collection of SamplePlayers available for realtime playback.
 */
class SampleManager
{
  public:

	SampleManager(class AudioPool* pool, class SampleConfig* samples);
	~SampleManager();

    // return true if the contents of this track are different
    // than what is defined in the SampleConfig
    // this was an optimization when we didn't have a more granular
    // way to update the sub-parts of the MobiusConfig, should
    // no longer be necessary and could have been done by the UI
    //bool isDifference(class SampleConfig* samples);

    int getSampleCount();

    // todo: supposed to respond to audio devlce changes
    // and adjust latency compensation
    void updateConfiguration(class MobiusConfig* config);

    /**
     * Trigger a sample and deposit the first block of content
     * in the interrupt buffer.  Return a pointer to the buffer
     * we decided to modify so Tracks can be notified that
     * something changed if they already started processing this buffer.
     */
    float* trigger(class MobiusAudioStream* stream, int index, bool down);
    float* trigger(MobiusAudioStream* stream, SamplePlayer* player, bool down);
    
    /**
     * Needed by scripts to wait for a trigger sample to finish.
     */
	long getLastSampleFrames();

    // called when buffers are available
    void processAudioStream(class MobiusAudioStream* stream);

    // player list only accessible for the Shell to build DynamicActions
    SamplePlayer* getPlayers() {
        return mPlayerList;
    }
    
  private:
	
	void init();

	SamplePlayer* mPlayerList;
	SamplePlayer* mPlayers[MAX_SAMPLES];
	int mSampleCount;
	int mLastSample;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
