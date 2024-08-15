/**
 * Utility class that sits between the plugin host and it's notion
 * of timekeeping, and the Mobius Synchronizer.
 *
 * JuceAudioStream handles interaction with the Juce objects to dig out
 * most of the available information from the AudioPlayHead and calls
 * two methods:
 *
 *     updateTempo
 *        Includes when available the BPM and time signature
 *
 *     advance
 *        Includes informatation about where this audio block is
 *        relative to the hosts's idea of time.  The most important
 *        is beatPosition which is the same as what VST2 called "PPQ position"
 *
 * There is other information available that hosts may provide that may be interesting
 * someday, such as whether the host is looping, the video frame rate, "origin time" which
 * I think is related to SMPTE.  
 *
 * ppqPosition drives everything.  It is a floating point number that represents
 * "the current play position in units of quarter notes".  There is some ambiguity
 * over how hosts implement the concepts of "beats" and "quarter notes" and they
 * are not always the same.  In 6/8 time, there are six beats per measure and the
 * eighth note gets one beat.  Unclear whether ppq means "pulses per beat" which
 * would be pulses per eights, or whether that would be adjusted for quarter notes.
 * Will have to experiment with differnent hosts to see what they do.
 *
 * ppqPosition normally starts at 0.0f when the transport starts and increase
 * on each block.  We know a beat happens when the non-fractional part of this
 * number changes, for example going from 1.xxxxx on the last block to 2.xxxxxx
 * But note that the beat actually happened in the PREVIOUIS block, not the
 * block being received.  It would be possible to use the sample rate to determine
 * whether the next beat MIGHT occur in the previous block and calculate a more
 * accurate buffer offset to where the beat actually is.  But this is fraught with
 * round off errors and edge conditions.
 *
 * The simplest thing is to do beat detection at the beginning of every block
 * when the integral value ppqPosition changes.  This in effect quantizes beats
 * to block boundaries, and makes Mobius a little late relative to the host.
 * With small buffer sizes, this difference is not usually noticeable audibly.
 *
 * What can be an issue is the resulting loop length will not exactly match the
 * hosts notion of the time between the starting and ending beats.  For example,
 * assume a 128 frame buffer where beats occur at sample 64 within that buffer.
 * When doing beat detection on the next buffer, the loop "lost" 64 frames from
 * the previous buffer that should have been included in the loop when initating
 * the recording.  When ending the recording, the loop will include 64 frames
 * too many.  Assuming tempo is not changing, errors should tend to cancel each other
 * but since we're dealing with floats there will always be small errors that
 * can lead to mismatched loop sizes, which causes drift over time.  The goal
 * is to make that drift as small as possible.
 *
 * Older code included at the bottom of this file tried to be smart about beat
 * offsets within the buffer, and adapt to anomolies in old hosts.  This is included
 * for reference only but it was buggy and trying too hard.
 *
 * Until this is shown to cause problems, beats and bars will always be
 * detected at the beginning of every buffer.
 *
 * The next problem are the relative beat and bar NUMBERS.
 *
 * Beat number is usually the integral portion of ppqPosition, though I don't know if
 * the spec requires that to be a monotonically increasing number or if just needs to
 * change to indiciate a beat has happened.  For most hosts this increases without bounds.
 * For FL Studio and probably other pattern-based hosts it returns to zero at the loop point.
 *
 * For Mobius synchronization with Sync=HostBeat it doesn't matter what the beat number is,
 * Mobius simply starts and ends on a beat.  Once the loop is recorded though, there can
 * be two ways to represent beat numbers: the beat number the host says it is on, and the
 * beat number within the Mobius loop relative to the start of that loop.  For example if you
 * start a recording on host beat 3 and end it on host beat 7.  What the host is displaying
 * as beats 12345678  would be Mobius beats 34123412
 *
 * This can result in confusion for anything in scripting that refers to the beat number
 * such as "wait for beat 3".   Is that beat 3 of the host, or is it the third beat within
 * the Mobius loop?  I think most users would expect this to work like subcycles, where
 * beats are numbered relative to the start of the Mobius loop.  This would also be necessary
 * for linear-based hosts that never loop back to beat 0 but just keep increasing their
 * beat counts forever.
 *
 * For pattern based hosts that do beat number looping, it would be interesting to allow
 * scripts to address them: "wait for Mobius beat 1" vs. "wait for host beat 3" which you
 * could use to retrigger the Mobius loop so that it starts on host beat 3 and realigns
 * the two beat numbers.
 * !! Yes, do this
 *
 * If Mobius displays Mobius relative beat numbers then there are two states for that display.
 * Before recording the loop, it displays host beat numbers, but once it has been recorded
 * it starts displaying Mobius beat numbers.
 *
 * Now we have BARS.
 *
 * There is marginal support for bars in VST3/Juce.  If the host returns a time signature
 * you can calculate the number of beats in each bar, except for the ambiguity over whether
 * the ppqPoition number is time signature beats, or quarter notes.  I'm starting by assuming
 * it is time signature beats, but this will need testing in various hosts.
 *
 * There doesn't appear to be a reliable way for the hosts to provide BAR NUMBERS.
 * Juce has a method that returns "the number of bars since the beginning of the timeline"
 * but not all hosts support that concept nor do all plugin formats.  Will have to test.
 * Even if they do provide a bar number, we have the same problem with the bar number the
 * host is on being different than the the bar within the loop Mobius is playing.
 * For example if a Mobius loop only has 4 bars, the user might expect to see bars 1234
 * repeating, especially if you display Mobius relative beat numbers.  Unclear what the
 * right thing to display is here, but it is clear that "beat/bar in the host" will be
 * different than "beat/bar within the Mobius loop" so both numbers need to be maintained
 * and accessible in scripts.  The UI can be configured to display one or the other.
 *
 * Finally we have the old beatsPerBar parameter in the Setup.  This is used for MIDI 1.0 has
 * no reliable concept of time signature.  That may still be used in older synchronization
 * code but we need to stop doing that for host sync.  Beats per bar should always
 * come from the host if it provides it and if not it can fall back to a Mobius parameter.
 * I don't think it is useful to override the host BPB if it does have one.
 *
 * ----------------------------------------------------------------------------------------
 *
 * There are two collections of code here, a newer one that is trying to simplify
 * how this shit works, and an older one that was used when Mobius 3 was first released.
 * The old one is only there for backup until the new one is working properly.
 *
 */

#include "util/Trace.h"

// for AudioTime
#include "mobius/MobiusInterface.h"

#include "HostSyncState.h"

//////////////////////////////////////////////////////////////////////
//
// New Implementation
//
//////////////////////////////////////////////////////////////////////

NewHostSyncState::NewHostSyncState()
{
}

NewHostSyncState::~NewHostSyncState()
{
}

/**
 * Update tempo state.
 * Same as the old one.
 */
void NewHostSyncState::updateTempo(int sampleRate, double tempo, 
                                   int numerator,  int denominator)
{
    bool tempoChanged = false;
    
    if (sampleRate != mSampleRate) {
		if (mTraceTempo)
		  Trace(2, "HostSync: Sample rate changing from %d to %d\n",
				mSampleRate, sampleRate);
        mSampleRate = sampleRate;
        tempoChanged = true;
    }

    if (tempo != mTempo) {
		if (mTraceTempo)
		  Trace(2, "HostSync: Tempo changing from %d to %d\n", (int)mTempo, (int)tempo);
        mTempo = tempo;
        tempoChanged = true;
    }

    // recalculate when any component changes
    if (tempoChanged) {
        int framesPerMinute = 60 * mSampleRate;
        double bpf = mTempo / (double)framesPerMinute;
        if (bpf != mBeatsPerFrame) {
			if (mTraceTempo)
			  Trace(2, "HostSync: BeatsPerFrame changing to %d\n", (int)bpf);
            mBeatsPerFrame = bpf;
        }
    }

    // !! Comments in VstMobius indiciate that denominator at least
    // can be fractional for things like 5/8.  Really!?

    bool tsigChange = false;

    if (numerator != mTimeSigNumerator) {
		if (mTraceTempo)
		  Trace(2, "HostSync: Time sig numerator changing to %d\n", numerator);
        mTimeSigNumerator = numerator;
        tsigChange = true;
    }

    if (denominator != mTimeSigDenominator) {
		if (mTraceTempo)
		  Trace(2, "HostSync: Time sig denominator changing to %d\n", denominator);
        mTimeSigDenominator = denominator;
        tsigChange = true;
    }
    
    if (tsigChange) {
        double bpb = mTimeSigNumerator / (mTimeSigDenominator / 4);
        if (bpb != mBeatsPerBar) {
			if (mTraceTempo)
			  Trace(2, "HostSync: BeatsPerBar changing to %d\n", (int)bpb);
            mBeatsPerBar = bpb;
        }
    }

    // warn if we see a fractional bpb because the calculations below aren't
    // prepared to deal with that
    if ((mBeatsPerBar - (float)((int)mBeatsPerBar)) != 0.0f)
      Trace(1, "HostSync: Looks like we have a fractional time signature");
    
}

/**
 * This is a very simplified implementation of the old one that just tries
 * to detect beat/bar boundaries and doesn't try to be too smart.
 */
void NewHostSyncState::advance(int frames,
                               bool transportPlaying,
                               double newSamplePosition, 
                               double newBeatPosition)
{
    // I don't think we need this any more
    (void)newSamplePosition;
    (void)frames;

    // at the end of this mess, these flags will be set to indiciate which events
    // to fire
    bool sendStop = false;
    bool sendStart = false;
    bool sendBeat = false;
    bool sendBar = false;

    // ponder various things the host is telling us and determine what
    // state we should be in, but don't act on it yet
    
    // first the easy part play/stop
    // will need to deal with pause/resume at some point
    bool started = false;
    bool stopped = false;
    if (mPlaying && !transportPlaying) {
        if (mTraceBeats)
          Trace(2, "HSS: Stop");
        stopped = true;
    }
    else if (!mPlaying && transportPlaying) {
        if (mTraceBeats)
          Trace(2, "HSS: Play");
        started = true;
    }
    
    // beat number is usually (always?) the integral portion of ppqpos
    int newBeat = (int)newBeatPosition;

    // this assumes that the host is moving the transport on even beat boundaries
    // and not continuously, which is not necessarily true
    // detecting that difference is subtle and what got us into trouble in the old code
    // until this becomes a problem and after basic beat tracking is working correctly
    // assume any change causes beat/bar events and let drift correction sort out the
    // problems later
    bool onBeat = started || (newBeat != mLastBeat);

    // detect jumps
    bool jump = false;
    if (newBeatPosition < mLastBeatPosition) {
        if (mTraceBeats)
          Trace(2, "HSS: Rewind");
        jump = true;
    }
    else {
        // forward detection requires looking at effective beat counts
        // though I suppose we could check for a partial advance within
        // a certain touchy threshold
        // if the beat is more than the expect 1 more, then they are touching the timeline
        if (newBeat > (mLastBeat + 1)) {
            if (mTraceBeats)
              Trace(2, "HSS: FastForward");
            jump = true;
        }
    }

    if (jump) {
        // what if anything to do here?  Could try to retrigger the Mobius loop
        // to align with the host transport but that's really complicated and moreso
        // if you consider the difference between a live loop playing and what
        // happens if the Mobius loop is paused, what jumps mean if the transport
        // is stopped vs. playing, and others...
        // if they start fiddling with the transport, Mobius will not track that
        // and will drift
    }
    
    // detect loops
    bool looping = false;
    if (newBeat < mLastBeat) {
        // we went backward by an amount equal to or greater than 1 beat
        // this commonly happens with FL Studio which jumps back to beat 0 when
        // it loops the pattern it is playing
        // jumping backward, even to zero, does not necessarily mean we're looping
        // so don't act on it
        if (newBeat == 0)
          looping = true;
    }
    else if (newBeat > (mLastBeat + 1)) {
        // likewise if the host is looping in reverse we could be counting down to zero
        // then jump back to the end, this is harder than looping to zero because we don't
        // know how long the looping pattern is.  Some hosts can give that to the plugins,
        // Juce has interfaces for host looping info.  We could also guess by monitoring the
        // beat numbers for a few cycles to see if they repeat consistently.
    }
    if (looping && mTraceBeats)
      Trace(2, "HSS: Might be looping");


    // bars are hard
    // IFF the host is giving us time signature information
    // then we can calculate what bar we are on, if it doesn't we could fall back
    // to whatever BPB is set in Mobius configuration but I don't think we should do that
    // here.  Capture whatever the host tells us the bar is, and do fancy Mobius transformation
    // down in Synchronizer
    // unfortunately that means if the host doesn't give us a timesig, we'll never be able
    // to use Sync=HostBar without the user configuring something, but in a way that's accurate
    // because if the host doesn't have the notion of a bar, what does bar sync even mean?
    int newBar = 0;
    bool onBar = false;
    if (mBeatsPerBar > 0) {
        // some VST comments indiciate that BPB can be fractional, I guess to represent
        // the difference between ppq meaning "quarter note" or "beat when in 6/8 time"
        // need to detect this
        newBar = (int)((float)newBeat / mBeatsPerBar);

        // doesn't work if we never get out of the first bar
        //onBar = onBeat && (newBar != mLastBar);

        // FL Studio in it's basic configuration loops within one bar
        // so since the bar number never changes bar detection by comparing bar numbers
        // doesn't work, I don't want to mess with counting beats so if we detect
        // a loop to zero consider that a bar
        //onBar = onBeat && (newBeat == 0);

        // no, this should handle both cases, right?
        // probably won't work if BPB is fractional
        if (onBeat) {
            int barBeat = (newBeat % (int)mBeatsPerBar);
            onBar = (barBeat == 0);
        }
    }
    
    //
    // Actions
    //

    if (stopped) {
        sendStop = true;
        // must reset this so we get a beat detection the next time the transport is resumed
        // !! unclear what to do here, the host transport is not guaranteed to resume
        // exactly on a beat
        mLastBeatPosition = -1.0f;
        mLastBeat = -1;
        mLastBar = -1;
    }
    else if (started) {
        // and here we have the "where are we when started" problem
        // for Mobius it doesn't really matter when the transport starts, just
        // when the next beat starts, and if you generate a beat event whenever the
        // transport starts that's enough.
        //
        // This is where we are likely to have problems.  The user could be using the transport
        // in two ways:
        //
        //     1) Start the transport with no Mobius recording armed.
        //        Mobius synchronizes to the next beat/bar after the start.
        //        This is easy.
        //
        //     2) Arm Mobius for sync recording, then start the transport.
        //        This is hard because we wake up with no awareness of where the transport
        //        beat is other than "it used to be nothing and now you're somewhere after
        //        beat 42".
        //
        // In case 2, if you wait until the next true beat detection there will be a gap
        // between when the transport starts and when Mobius starts and when the host started
        // which is probably not what you want.
        // If you start Mobius immediately, the host may not actually be on a beat at that
        // point, and the next beat will be detected earlier than it normally would be which
        // will confuse the sync tracker.
        //
        // Even if you work out the initial instability in beat widths, Mobius will end the
        // recording on a TRUE beat so the loop length will be wrong.  You would have to actually
        // end it a little late.  The distance between when we started and the next true beat
        // detection.
        //
        // Unclear what the better approach is here.  I think most users are not goin
        // to be randomly fiddling with the timeline.  When it starts it is almost always
        // exactly on beat zero, or on a bar boundary.
        sendStart = true;

        if (onBar)
          sendBar = true;
        else
          sendBeat = true;
    }
    else if (mPlaying) {
        if (onBar)
          sendBar = true;
        else if (onBeat)
          sendBeat = true;
    }

    if (mTraceBeats) {
        if (sendStart)
          Trace(2, "HSS: Start event");
        if (sendStop)
          Trace(2, "HSS: Stop event");
        if (sendBeat)
          Trace(2, "HSS: Beat event");
        if (sendBar)
          Trace(2, "HSS: Bar event");
    }

    mLastBeat = newBeat;
    mLastBar = newBar;
    mLastBeatPosition = newBeatPosition;
    mPlaying = transportPlaying;
    mBeatBoundary = onBeat;
    mBarBoundary = onBar;
}      

void NewHostSyncState::transfer(AudioTime* autime)
{
	autime->tempo = mTempo;
    autime->beatPosition = mLastBeatPosition;
	autime->playing = mPlaying;
    autime->beatBoundary = mBeatBoundary;
    autime->barBoundary = mBarBoundary;
    autime->boundaryOffset = 0;
    autime->beat = mLastBeat;
    autime->bar = mLastBar;
    // can this ever be fractional?
	autime->beatsPerBar = (int)mBeatsPerBar;
}

//////////////////////////////////////////////////////////////////////
//
// Old Implementation
//
//////////////////////////////////////////////////////////////////////

/**
 * The -1 initializations are there because the initial beat after
 * starting is usually at ppq 0.0f and that needs to be detected as
 * a beat boundary.
 *
 * mLastSamplePosition is only relevant when trying to detect transport
 * changes from the sample position.
 *
 * Since we don't reset sync state when the transport stops, we're
 * in a very small "unknown" state at the beginning.  Feels better just
 * to assum we're at zero?
 */
HostSyncState::HostSyncState()
{
	// changes to stream state
	mTraceChanges = true;
	// SyncTracker traces enough, don't need this too if  
	// things are working
    mTraceBeats = false;

    // These were options for a few ancient hosts and some weird
    // Cubase behavior
    mHostRewindsOnResume = false;
    mHostPpqPosTransport = false;
    mHostSamplePosTransport = false;

    mSampleRate = 0;
    mTempo = 0;
    mTimeSigNumerator = 0;
    mTimeSigDenominator = 0;
    mBeatsPerFrame = 0.0;
    mBeatsPerBar = 0.0;

    mPlaying = false;
    mTransportChanged = false;
    mLastSamplePosition = -1.0;
    mLastBeatPosition = -1.0;

    mResumed = false;
    mStopped = false;
    mAwaitingRewind = false;

    mLastBeatRange = 0.0;
    mBeatBoundary = false;
    mBarBoundary = false;
    mBeatOffset = 0;
    mLastBeat = -1;
    mBeatCount = 0;
    mBeatDecay = 0;

    // new
    mLastBaseBeat = 0;
}

HostSyncState::~HostSyncState()
{
}

/**
 * Don't have this any more.  Wait until a host needs special
 * treatment before resurrecting it.
 */
#if 0
void HostSyncState::setHost(HostConfigs* config)
{
	if (config != NULL) {
        mHostRewindsOnResume = config->isRewindsOnResume();
        mHostPpqPosTransport = config->isPpqPosTransport();
        mHostSamplePosTransport = config->isSamplePosTransport();
    }
}
#endif

void HostSyncState::setHostRewindsOnResume(bool b)
{
	mHostRewindsOnResume = b;
}

/**
 * Export our sync state to an AudioTime.
 * There is model redundancy here, but I don't want
 * AudioTime to contain the method implementations and there
 * is more state we need to keep in HostSyncState.
 */
void HostSyncState::transfer(AudioTime* autime)
{
	autime->tempo = mTempo;
    autime->beatPosition = mLastBeatPosition;
	autime->playing = mPlaying;
    autime->beatBoundary = mBeatBoundary;
    autime->barBoundary = mBarBoundary;
    autime->boundaryOffset = mBeatOffset;
    autime->beat = mLastBeat;
    // can this ever be fractional?
	autime->beatsPerBar = (int)mBeatsPerBar;
}

/**
 * Update tempo state.
 */
void HostSyncState::updateTempo(int sampleRate, double tempo, 
                                int numerator,  int denominator)
{
    bool tempoChanged = false;
    
    if (sampleRate != mSampleRate) {
		if (mTraceChanges)
		  Trace(2, "HostSync: Sample rate changing from %d to %d\n",
				mSampleRate, sampleRate);
        mSampleRate = sampleRate;
        tempoChanged = true;
    }

    if (tempo != mTempo) {
		if (mTraceChanges)
		  Trace(2, "HostSync: Tempo changing from %d to %d\n", (int)mTempo, (int)tempo);
        mTempo = tempo;
        tempoChanged = true;
    }

    // recalculate when any component changes
    if (tempoChanged) {
        int framesPerMinute = 60 * mSampleRate;
        double bpf = mTempo / (double)framesPerMinute;
        if (bpf != mBeatsPerFrame) {
			if (mTraceChanges)
			  Trace(2, "HostSync: BeatsPerFrame changing to %d\n", (int)bpf);
            mBeatsPerFrame = bpf;
        }
    }

    // !! Comments in VstMobius indiciate that denominator at least
    // can be fractional for things like 5/8.  Really!?

    bool tsigChange = false;

    if (numerator != mTimeSigNumerator) {
		if (mTraceChanges)
		  Trace(2, "HostSync: Time sig numerator changing to %d\n", numerator);
        mTimeSigNumerator = numerator;
        tsigChange = true;
    }

    if (denominator != mTimeSigDenominator) {
		if (mTraceChanges)
		  Trace(2, "HostSync: Time sig denominator changing to %d\n", denominator);
        mTimeSigDenominator = denominator;
        tsigChange = true;
    }
    
    if (tsigChange) {
        double bpb = mTimeSigNumerator / (mTimeSigDenominator / 4);
        if (bpb != mBeatsPerBar) {
			if (mTraceChanges)
			  Trace(2, "HostSync: BeatsPerBar changing to %d\n", (int)bpb);
            mBeatsPerBar = bpb;
        }
    }
}

/**
 * Update stream state.
 *
 * "frames" is the number of frames in the current audio buffer.
 *
 * newSamplePosition is what VST calls "samplePos" and what AU calls 
 * currentSampleInTimeLine.  It increments on each buffer relative
 * to the start of the tracks which is sample zero.
 * 
 * newBeatPosition is what VST calls "ppqPos" and what AU calls "currentBeat".
 * It is a fractional beat counter relative to the START of the current buffer.
 * 
 * transportChanged and transportPlaying are true if the host can provide them.
 * Some hosts don't so we can detect transport changes based on changes
 * in the beatPosition or samplePosition.
 *
 * new: Juce may do the transport detection now...
 *
 */
void HostSyncState::advance(int frames, 
                            double newSamplePosition, 
                            double newBeatPosition,
                            bool transportChanged, 
                            bool transportPlaying)
{
    // update transport related state
    // sets mPlaying, mResumed, mStopped 
    updateTransport(newSamplePosition, newBeatPosition, 
                    transportChanged, transportPlaying);

    bool traceBuffers = false;
    if (traceBuffers && mPlaying) {
        Trace(2, "HostSync: samplePosition %d beatPosition %d frames %d\n", 
              (int)newSamplePosition, (int)newBeatPosition, frames);
    }

    // kludge for Cubase that likes to rewind AFTER the transport 
    // status changes to play
    if (mResumed) {
        if (mHostRewindsOnResume) {
			if (mTraceChanges)
			  Trace(2, "HostSync: awaiting rewind\n");
            mAwaitingRewind = true;
        }
    }
    else if (mStopped) {
        // clear this?  I guess it doesn't matter since
        // we'll set it when we're resumed and we don't
        // care when !mPlaying
        mAwaitingRewind = false;
    }
    else if (mAwaitingRewind) {
        if (mLastBeatPosition != newBeatPosition) {
            mAwaitingRewind = false;
            // make it look like a resume for the beat logic below
            mResumed = true;
			if (mTraceChanges)
			  Trace(2, "HostSync: rewind detected\n");
        }
    }

    // set if we detect a beat in this buffer
    // don't trash mBeatBoundary yet, we still need it 
    bool newBeatBoundary = false;
    bool newBarBoundary = false;
    long newBeatOffset = 0;
    double newBeatRange = 0.0;

    // remove the fraction
    long baseBeat = (long)newBeatPosition;
    
    // Determine if there is a beat boundary in this buffer
    if (mPlaying && !mAwaitingRewind) {

        long newBeat = baseBeat;

        // determine the last ppqPos within this buffer
        newBeatRange = newBeatPosition + (mBeatsPerFrame * (frames - 1));

        // determine if there is a beat boundary at the beginning
        // or within the current buffer, and set beatBoundary
        if (newBeatPosition == (double)newBeat) {
            // no fraction, first frame is exactly on the beat
            // NOTE: this calculation, like any involving direct equality
            // of floats may fail due to rounding error, in one case
            // AudioMulch seems to reliably hit beat 128 with a ppqPos
            // of 128.00000000002 this will have to be caught in
            // the jump detector below, which means we really don't
            // need this clause
            if (!mBeatBoundary)
              newBeatBoundary = true;
            else { 
                // we advanced the beat in the previous buffer,
                // must be an error in the edge condition?
                // UPDATE: this might happen due to float rounding
                // so we should probably drop it to level 2?
                Trace(1, "HostSync: Ignoring redundant beat edge condition!\n");
            }
        }
        else {
            // detect beat crossing within this buffer
            long lastBeatInBuffer = (long)newBeatRange;
            if (baseBeat != lastBeatInBuffer ||
                // fringe case, crossing zero
                (newBeatPosition < 0 && newBeatRange > 0)) {
                newBeatBoundary = true;
                newBeatOffset = (long)
                    (((double)lastBeatInBuffer - newBeatPosition) / mBeatsPerFrame);
                newBeat = lastBeatInBuffer;
            }
        }

        // sanity check on this
        bool missedBeat = false;
        if ((mLastBaseBeat != baseBeat) &&
            !newBeatBoundary) {
            Trace(1, "HostSync: Looks like we missed a beat");
            missedBeat = true;
        }

        // check for jumps and missed beats
        // when checking forward movement look at beat counts rather
        // than expected beatPosition to avoid rounding errors
        bool jumped = false;
        if (newBeatPosition <= mLastBeatPosition) {
            // the transport was rewound, this happens with some hosts
            // such as Usine that maintain a "cycle" and wrap the
            // beat counter from the end of the cycle back to the front
			if (mTraceChanges)
			  Trace(2, "HostSync: Transport was rewound\n");
            jumped = true;
        }
        else if (newBeat > (mLastBeat + 1)) {
            // a jump of more than one beat, transport must be forwarding
			if (mTraceChanges)
			  Trace(2, "HostSync: Transport was forwarded\n");
            jumped = true;
        }
        else if (!newBeatBoundary && (newBeat != mLastBeat)) {
            // A single beat jump, without detecting a beat boundary.
            // This can happen when the beat falls exactly on the first
            // frame of the buffer, but due to float rounding we didn't
            // catch it in the (beatPosition == (double)newBeat) clause above.
            // In theory, we should check to see if mLastBeatRange is
            // "close enough" to the current beatPosition to prove they are
            // adjacent, otherwise, we could have done a fast forward
            // from the middle of the previous beat to the start of this 
            // one, and should treat that as a jump?  I guess it doesn't
            // hurt the state machine, we just won't get accurately sized
            // loops if we're doing sync at the moment.
            if (!mBeatBoundary)
              newBeatBoundary = true;
            else {
                // this could only happen if we had generated a beat on the
                // previous buffer, then instantly jumped to the next beat
                // it is a special case of checking mLastPpqRange, the
                // two buffers cannot be adjacent in time
				if (mTraceChanges)
				  Trace(2, "HostSync: Transport was forwarded one beat\n");
                jumped = true;
            }
        }

        if (missedBeat && newBeatBoundary)
          Trace(1, "HostSync: Missed beat corrected");

        // when we resume or jump, have to recalculate the beat counter
        if (mResumed || jumped) {
            // !! this will be wrong if mBeatsPerBar is not an integer,
            // when would that happen?
            mBeatCount = (int)(baseBeat % (long)mBeatsPerBar);
			if (mTraceChanges) {
				if (mResumed)
				  Trace(2, "HostSync: Resuming playback at bar beat %d\n",
						mBeatCount);
				else 
				  Trace(2, "HostSync: Playback jumped to bar beat %d\n",
						mBeatCount);
			}
        }

        // For hosts like Usine that rewind to the beginning of a cycle,
        // have to suppress detection of the beat at the start of the
        // cycle since we already generated one for the end of the cycle
        // on the last buffer.  This will also catch odd situations
        // like instantly moving the location from one beat to another.
        if (newBeatBoundary) {
            if (mBeatBoundary) {
                // had one on the last buffer
                Trace(1, "HostSync: Canceling beat boundary for some reason");
                newBeatBoundary = false;
                if (!mResumed && !jumped) 
                  Trace(1, "HostSync: Supressed double beat, possible calculation error!\n");
                // sanity check, mBeatDecay == 0 should be the same as
                // mBeatBoundary since it happened on the last buffer
                if (mBeatDecay != 0)
                  Trace(1, "HostSync: Unexpected beat decay value!\n");
            }
            else {
                int minDecay = 4; // need configurable maximum?
                if (mBeatDecay < minDecay) {
                    // We generated a beat/bar a few buffers ago, this
                    // happens in Usine when it rewinds to the start
                    // of the cycle, but let's it play a buffer past the
                    // end of the cycle before rewinding.  This is a host
                    // error since the bar length Mobius belives is
                    // actually shorter than the one Usine will be playing.
                    Trace(1, "HostSync: Suppressed double beat, host is not advancing the transport correctly!\n");
                    newBeatBoundary = false;
                }
            }
        }


        // Detect bars
        // VST barStartPos is useless because hosts don't implement
        // it consistently, see vst notes for more details.
        if (newBeatBoundary) {
            if ((mResumed || jumped) && newBeatOffset == 0) {
                // don't need to update the beat counter, but we may
                // be starting on a bar
                if (mBeatCount == 0 || mBeatCount >= mBeatsPerBar) {
                    newBarBoundary = true;
                    mBeatCount = 0;
                }
            }
            else {
                mBeatCount++;
                if (mBeatCount >= mBeatsPerBar) {
                    newBarBoundary = true;
                    mBeatCount = 0;
                }
            }
        }

        // selectively enable these to reduce clutter in the stream
        if (mTraceBeats) {
            if (newBarBoundary)
              Trace(2, "HostSync: BAR: position: %d range: %d offset %d\n", 
                    (int)newBeatPosition, (int)newBeatRange, newBeatOffset);

            else if (newBeatBoundary)
              Trace(2, "HostSync: BEAT: postition: %d range: %d offset %d\n", 
                    (int)newBeatPosition, (int)newBeatRange, (int)newBeatOffset);
        }

        mLastBeat = (int)newBeat;
    }

    // save state for the next interrupt
    mLastBaseBeat = (int)baseBeat;
    mLastSamplePosition = newSamplePosition;
    mLastBeatPosition = newBeatPosition;
    mLastBeatRange = newBeatRange;
    mBeatBoundary = newBeatBoundary;
    mBarBoundary = newBarBoundary;
    mBeatOffset = (int)newBeatOffset;

    if (mBeatBoundary)
      mBeatDecay = 0;
    else
      mBeatDecay++;
}

/**
 * Update state related to host transport changes.
 */
void HostSyncState::updateTransport(double samplePosition, 
                                    double beatPosition,
                                    bool transportChanged, 
                                    bool transportPlaying)
{
    mResumed = false;
    mStopped = false;

    // detect transport changes
	if (transportChanged) {
        Trace(2, "HostSync: transportChanged");
		if (transportPlaying != mPlaying) {
			if (transportPlaying) {
				if (mTraceChanges)
				  Trace(2, "HostSync: PLAY\n");
				mResumed = true;
			}
			else {
				if (mTraceChanges)
				  Trace(2, "HostSync: STOP\n");
				// Clear out all sync status
				// or just keep going pretending there are beats and bars?
                mStopped = true;
			}
			mPlaying = transportPlaying;
		}
		else {
			// shouldn't be getting redundant signals?
		}
	}
    else if (mHostSamplePosTransport) {
        // set only for hosts that don't reliably do transport 
		if (mLastSamplePosition >= 0.0) { 
			bool playing = (mLastSamplePosition != samplePosition);
			if (playing != mPlaying) {
				mPlaying = playing;
				if (mPlaying) {
					if (mTraceChanges)
					  Trace(2, "HostSync: PLAY (via sample position) %d %d\n",
							(int)mLastSamplePosition,
							(int)samplePosition);
					mResumed = true;
				}
				else {
					if (mTraceChanges)
					  Trace(2, "HostSync: STOP (via sample position)\n");
					// Clear out all sync status
					// or just keep going pretending there are beats and bars?
                    mStopped = true;
				}
			}
		}
	}
	else if (mHostPpqPosTransport) {
        // Similar to mCheckSamplePosTransport we could try to detect
        // this with movement of ppqPos.  This seems even less likely
        // to be necessary
		if (mLastBeatPosition >= 0.0) {
			bool playing = (mLastBeatPosition != beatPosition);
			if (playing != mPlaying) {
				mPlaying = playing;
				if (mPlaying) {
					if (mTraceChanges)
					  Trace(2, "HostSync: PLAY (via beat position) %d %d\n",
							(int)mLastBeatPosition, (int)beatPosition);
					mResumed = true;
				}
				else {
					if (mTraceChanges)
					  Trace(2, "HostSync: STOP (via beat position)\n");
                    mStopped = true;
				}
			}
		}
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
