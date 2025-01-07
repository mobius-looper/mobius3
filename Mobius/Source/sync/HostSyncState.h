/**
 * Old utility class to convert plugin host time information into the AudioTime
 * model used by Mobius/Synchronizer.
 *
 * Old comments:
 * A generic representation of host synchronization state.
 * Besides maintaining sync state, this is also where we implement
 * the beat detction algorithm since it is the same for AU and VST.
 *
 * Much of what is in here is the same as AudioTime but we keep 
 * extra state that we don't want to expose to the plugin.
 */

#pragma once

/**
 * An emerging replacement for the older HostSyncState below
 * these will be maintained in parallel for awhile.
 */
class NewHostSyncState {

  public:

    NewHostSyncState();
    ~NewHostSyncState();

    /**
     * Update state related to host tempo and time signature.
     */
    void updateTempo(int sampleRate, double tempo, 
                     int timeSigNumerator, int timeSigDenomonator);

    /**
     * Update audio stream state.
     */
    void advance(int frames, bool transportPlaying, double samplePosition, double beatPosition);

    void transfer(class HostAudioTime* autime);
    
  private:

    bool mTraceTempo = true;
    bool mTraceBeats = false;
    
    /**
     * The current sample rate reported by the host.  This is not
     * expected to change though we track it.
     */
    int mSampleRate = 0;
    
    /** 
     * The current tempo reported by the host.  This is expected to change.
     */
    double mTempo = 0.0f;

    /**
     * The current time signagure reported by the host.
     */
    int mTimeSigNumerator = 0;
    int mTimeSigDenominator = 0;
    double mBeatsPerBar = 0.0f;
    
    // 
    // Things derived from updateTempo()
    //

    /**
     * The fraction of a beat represented by one frame.
     * Typically a very small number.  This is used in the conversion
     * of mBeatPosition into a buffer offset.
     * Don't need this any more if we just let beats be quantized to the start
     * of every block.
     */
    double mBeatsPerFrame = 0.0f;

    //
    // Transport State
    //

    bool mPlaying = false;
    double mLastBeatPosition = -1.0f;
    int mLastBeat = -1;
    int mLastBar = -1;
    bool mBeatBoundary = false;
    bool mBarBoundary = false;
    
};

///////////////////////////////////////////////////////////////////////
//
// The first varient I started using during the Juce port
// Don't remember why this was deprecated but it hasn't been used in awhile
//
///////////////////////////////////////////////////////////////////////


class OldHostSyncState {

  public:
    
    OldHostSyncState();
    ~OldHostSyncState();

    // temporary: maintain this in parallel for awhile
    NewHostSyncState newState;

	/**
	 * Adjust for optional host options.
	 */
	//void setHost(class HostConfigs* config);
	void setHostRewindsOnResume(bool b);

    /**
     * Update state related to host tempo and time signature.
     */
    void updateTempo(int sampleRate, double tempo, 
                     int timeSigNumerator, int timeSigDenomonator);

    /**
     * Update audio stream state.
     */
    void advance(int frames, double samplePosition, double beatPosition,
                 bool transportChanged, bool transportPlaying);

    /**
     * Transfer our internal state into an AudioTime for the plugin.
     */
    void transfer(class HostAudioTime* autime);

    /**
     * new: so we can implement the transportChanged flag since
     * it is gone in VST3/AU3
     */
    bool isPlaying() {
        return mPlaying;
    }

  private:

    void updateTransport(double samplePosition, double beatPosition,
                         bool transportChanged, bool transportPlaying);


	/**
	 * True to enable general state change trace.
	 */
	bool mTraceChanges;

    /**
     * True to enable beat trace.
     */
    bool mTraceBeats;

    //
    // things copied from HostConfig
    //

    /**
     * When true it means that the host transport rewinds a bit after a resume.
     * This was noticed in an old version of Cubase...
     *
     * "Hmm, Cubase as usual throws a wrench into this.  Because of it's odd
     * pre-roll, ppqPos can actually go negative briefly when starting from
     * zero. But it is -0.xxxxx which when you truncate is just 0 so we can't
     * tell when the beat changes given the lastBeat formula above."
     *
     * When this is set it tries to compensate for this pre-roll, not sure
     * if modern versions of Cubase do this.
     */
    bool mHostRewindsOnResume;

    /**
     * When true, we check for stop/play by monitoring the ppqPos
     * rather than expecting kVstTransportChanged events.
     * This was originally added for Usine around 2006, not sure if
     * it's still necessary.
     */
    bool mHostPpqPosTransport;

    /**
     * When true we check for stop/play by monitoring the samplePos.
     * rather than expecting kVstTransportChanged events.
     * This was added a long time ago and hasn't been enabled for several
     * releases.
     */
    bool mHostSamplePosTransport;

    //
    // Things passed to updateTempo()
    //

    /**
     * The current sample rate reported by the host.  This is not
     * expected to change though we track it.
     */
    int mSampleRate;
    
    /** 
     * The current tempo reported by the host.  This is expected to change.
     */
    double mTempo;

    /**
     * The current time signagure reported by the host.
     */
    int mTimeSigNumerator;
    int mTimeSigDenominator;

    // 
    // Things derived from updateTempo()
    //

    /**
     * The fraction of a beat represented by one frame.
     * Typically a very small number.  This is used in the conversion
     * of mBeatPosition into a buffer offset.
     */
    double mBeatsPerFrame;

    /**
     * Calculated from TimeSigNumerator and TimeSigDenominator.
     *
     *   bpb = mTimeSigNumerator / (mTimeSigDenominator / 4);
     * 
     * What is this doing!?  
     */
    double mBeatsPerBar;

    //
    // Things passed to adavnce()
    //

    /**
     * True if the transport is currently playing.
     */
    bool mPlaying;

    /**
     * True if the transport changed from playing to not playing
     * on the last block.
     */
    bool mTransportChanged;

    /**
     * The sample position of the last buffer.  This is normally
     * advances by the buffer size with zero being the start of the
     * host's timeline.
     */
    double mLastSamplePosition;

    /**
     * The beat position of the last buffer.  The integer portion
     * of this number is the current beat number in the host transport.
     * The fractional portion represents the distance to the next beat
     * boundary.  In VST this is ppqPos, in AU this is currentBeat.
     */
    double mLastBeatPosition;

    //
    // State derived from advance()
    //

    /**
     * Becomes true if the transport was resumed in the current buffer.
     */
    bool mResumed;

    /**
     * Becomes true if the transport was stopped in the current buffer.
     */
    bool mStopped;

    /**
     * Kludge for Cubase that likes to rewind AFTER 
     * the transport status changes to play.  Set if we see the transport
     * change and mRewindsOnResume is set.
     */
    bool mAwaitingRewind;

    /**
     * The beat range calculated on the last buffer.
     * Not actually used but could be to detect some obscure
     * edge conditions when the transport is jumping around.
     */
    double mLastBeatRange;

    /**
     * Becomes true if there is a beat within the current buffer.
     */
    bool mBeatBoundary;

    /**
     * Becomes true if there is a bar within the current buffer.
     * mBeatBoundary will also be true.
     */
    bool mBarBoundary;

    /**
     * The offset into the buffer of the beat/bar.
     */
    int mBeatOffset;

    /**
     * The last integer beat we detected.
     */
    int mLastBeat;

    /**
     * The beat count relative to the start of the bar.
     * The downbeat of the bar is beat zero.
     */
    int mBeatCount;

    /**
     * The number of buffers since the last one with a 
     * beat boundary.  Used to suppress beats that come in 
     * too quickly when the host transport isn't implemented properly.
     * This was for an old Usine bug.
     */
    int mBeatDecay;

    int mLastBaseBeat = 0;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
