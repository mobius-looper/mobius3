/**
 * Utility class that sits between the plugin host and it's notion
 * of timekeeping, and the Mobius Synchronizer.
 *
 * Basically what this does is detect when the host transport starts and
 * stops and converts the "ppq position" at the start of each audio block
 * into beat and bar counters and more precice offsets into the audio
 * block where those events occur.  It evolved over a number of years
 * using old hosts that worked differently and strangely, and probably
 * don't even exist any more.  Much of the logic is probably not necessary
 * any more, but it is touchy and I'm leaving as it was for now.
 *
 * Juce handles host transport start/stop now so that part at least isn't
 * necessary.
 *
 * Formerly used a model called HostConfig to adjust behavior for certain
 * hosts.  I'm not bringing that forward.
 */

#include "util/Trace.h"

// for AudioTime
#include "mobius/MobiusInterface.h"

#include "HostSyncState.h"

/**
 * The three initializations to -1 have been done
 * for a long time but I don't think they're all necessary.
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
    mTraceBeats = true;

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
 * Update tempo related state.
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
    mLastBaseBeat = baseBeat;
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
