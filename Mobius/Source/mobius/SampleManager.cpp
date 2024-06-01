/**
 * Manages a collection of audio fragments that may be injected into
 * the real-time audio stream.  This was developed for Mobius testing.
 *
 * In old code this was wound up in the Recorder model which has been removed.
 * It is now a relatlvey standalone component managed directly by
 * MobiusKernel.
 * 
 * Outstanding issues:
 * 
 * SamplePlayer wants to receive notifications when the input/output
 * latencies change
 *
 * The notion of SampleTrigger might not be necessary any more,
 * comments inticate it was at least partially replaced by Actions
 *
 * SampleManager::updateConfiguration
 *       void updateConfiguration(class MobiusConfig* config);
 *   what does it do?
 *
 * Some lingering ambiguity over how "record" cursors are supposed to
 * work.  Since behavior is expected by the unit tests, don't redesign
 * anything right now, but need to revisit this if this evolves into
 * a more flexible sample player.
 * 
 * ---
 * 
 * To make a cleaner separation between real-time and "UI" code,
 * the samples are read from files and the SampleManager is constructed
 * by SampleLoader outside of the audio thread.  Any code within
 * SampleManager can be assumed to be running in the audio thread.
 *
 */

#include "../util/Trace.h"
#include "../util/Util.h"
#include "../model/MobiusConfig.h"
#include "../model/SampleConfig.h"

#include "MobiusInterface.h"

#include "Audio.h"

#include "SampleManager.h"

//////////////////////////////////////////////////////////////////////
//
// SamplePlayer
//
//////////////////////////////////////////////////////////////////////

const char* SamplePlayer::getFilename()
{
    return mFilename;
}

void SamplePlayer::setNext(SamplePlayer* sp)
{
	mNext = sp;
}

SamplePlayer* SamplePlayer::getNext()
{
	return mNext;
}

void SamplePlayer::setAudio(Audio* a)
{
	mAudio = a;
}

Audio* SamplePlayer::getAudio()
{
	return mAudio;
}

void SamplePlayer::setSustain(bool b)
{
	mSustain = b;
}

bool SamplePlayer::isSustain()
{
	return mSustain;
}

void SamplePlayer::setLoop(bool b)
{
	mLoop = b;
}

bool SamplePlayer::isLoop()
{
	return mLoop;
}

void SamplePlayer::setConcurrent(bool b)
{
	mConcurrent = b;
}

bool SamplePlayer::isConcurrent()
{
	return mConcurrent;
}

long SamplePlayer::getFrames()
{
	long frames = 0;
	if (mAudio != nullptr)
	  frames = mAudio->getFrames();
	return frames;
}

/**
 * Incorporate changes made to the global configuration.
 * Trying to avoid a Mobius dependency here so pass in what we need.
 *
 * TODO: Latency can't vary on a per-Sample basis so this needs
 * to be held in SampleManager.
 */
void SamplePlayer::updateConfiguration(int inputLatency, int outputLatency)
{
    mInputLatency = inputLatency;
    mOutputLatency = outputLatency;
}

/**
 * Old code tried to deal with keyboard audo-repeat.  This is done
 * at a higher level now so some of this isn't relevant.
 *
 * Old comments:
 * If this is bound to the keyboard, auto-repeat will keep
 * feeding us triggers rapidly.  If this isn't a sustain sample,
 * then assume this means we're supposed to restart.  If it is a
 * sustain sample, then we need to wait for an explicit up trigger.
 * This state has to be held even after a non-loop sample has finished
 * playing and become inactive.
 */
void SamplePlayer::trigger(bool down)
{

	// !! still having the auto-repeat problem with non-sustained
	// concurrent samples

    bool doTrigger = false;
    if (down) {
        if (!mDown || !mSustain)
          doTrigger = true;
        mDown = true;
    }
    else {
        // only relevant for sustained samples
        if (mSustain)
          doTrigger = true;
        mDown = false;
    }

    if (doTrigger) {
        int nextTail = mTriggerTail + 1;
        if (nextTail >= MAX_TRIGGERS)
          nextTail = 0;

        if (nextTail == mTriggerHead) {
            // trigger overflow, audio must be unresponsive or
            // we're receiving triggers VERY rapidly, would be nice
            // to detect unresponse audio and just start ignoring
            // triggers
            Trace(1, "SamplePlayer::trigger trigger overflow\n");
        }
        else {
            // eventually have other interesting things here, like key
            mTriggers[mTriggerTail].down = down;
            mTriggerTail = nextTail;
        }

    }
}

/**
 * Play/Record the sample.
 * 
 * Playback is currently inaccurate in that we'll play from the beggining
 * when we should logically start from mOutputLatency in order to synchronize
 * the recording with the output.  
 *
 * Recording is done accurately.  The frame counter is decremented by
 * mInputLatency, and when this goes positive we begin filling the input 
 * buffer.
 */
void SamplePlayer::play(float* inbuf, float* outbuf, long frames)
{
    // process triggers
    while (mTriggerHead != mTriggerTail) {
        SampleTrigger* t = &mTriggers[mTriggerHead++];
		if (mTriggerHead >= MAX_TRIGGERS)
		  mTriggerHead = 0;

        if (!t->down) {
            if (mConcurrent) {
                // the up transition belongs to the first cursor
                // that isn't already in the process of stopping
                for (SampleCursor* c = mCursors ; c != nullptr ; 
                     c = c->getNext()) {
                    if (!c->isStopping()) {
                        c->stop();
                        break;
                    }
                }
            }
            else {
                // should be only one cursor, make it stop
                if (mCursors != nullptr)
                  mCursors->stop();
            }
        }
        else if (mConcurrent) {
            // We start another cursor and let the existing ones finish
            // as they may.  Keep these ordered.
            SampleCursor* c = newCursor();
			SampleCursor* last = nullptr;
            for (last = mCursors ; last != nullptr && last->getNext() != nullptr ; 
                 last = last->getNext());
            if (last != nullptr)
              last->setNext(c);
            else
              mCursors = c;
        }
        else {
            // stop existing cursors, start a new one
            // the effect is similar to a forced up transition but
            // we want the current cursor to end cleanly so that it
            // gets properly recorded and fades nicely.

            SampleCursor* c = newCursor();
			SampleCursor* last = nullptr;
            for (last = mCursors ; last != nullptr && last->getNext() != nullptr ; 
                 last = last->getNext()) {
                // stop them while we look for the last one
                last->stop();
            }
            if (last != nullptr)
              last->setNext(c);
            else
              mCursors = c;
        }
    }

    // now process cursors

    SampleCursor* prev = nullptr;
    SampleCursor* next = nullptr;
    for (SampleCursor* c = mCursors ; c != nullptr ; c = next) {
        next = c->getNext();

        c->play(inbuf, outbuf, frames);
        if (!c->isStopped())
          prev = c;
        else {
            // splice it out of the list
            if (prev == nullptr)
              mCursors = next;
            else
              prev->setNext(next);
            freeCursor(c);
        }
    }
}

/**
 * Allocate a cursor.
 * Keep these pooled since there are several things in them.
 * Ideally there should be only one pool, but we would have
 * to root it in SampleManager and pass it down.
 *
 * TODO: Dynamic memory allocation
 * Keep for now since this is mostly a testing tool, but
 * should be using a proper pool with allocations managed in the shell.
 */
SampleCursor* SamplePlayer::newCursor()
{
    SampleCursor* c = mCursorPool;
    if (c == nullptr) {
        c = new SampleCursor(this);
    }
    else {
        mCursorPool = c->getNext();
		c->setNext(nullptr);
        c->setSample(this);
    }
    return c;
}

/**
 * Return a cursor to the pool.
 */
void SamplePlayer::freeCursor(SampleCursor* c)
{
    c->setNext(mCursorPool);
    mCursorPool = c;
}

//////////////////////////////////////////////////////////////////////
//
// SampleCursor
//
//////////////////////////////////////////////////////////////////////

/*
 * Each cursor represents the playback of one trigger of the
 * sample.  To implement the insertion of the sample into
 * the recorded audio stream, we'll actually maintain two cursors.
 * The outer cursor handles the realtime playback of the sample, 
 * the inner cursor handles the "recording" of the sample into the
 * input stream.  
 *
 * Implementing this as cursor pairs was easy since they have to 
 * do almost identical processing, and opens up some interesting
 * possibilities.
 */

void SampleCursor::setNext(SampleCursor* c)
{
    mNext = c;
}

SampleCursor* SampleCursor::getNext()
{
    return mNext;
}

/**
 * Reinitialize a pooled cursor.
 *
 * The logic is quite contorted here, but every cursor appears to 
 * have an embedded record cursor.  
 */
void SampleCursor::setSample(SamplePlayer* s)
{
    mSample = s;
	mAudioCursor->setAudio(mSample->getAudio());
    mStop = false;
    mStopped = false;
    mMaxFrames = 0;

    if (mRecord != nullptr) {
        // we're a play cursor
        mRecord->setSample(s);
        mFrame = 0;
    }
    else {
        // we're a record cursor

		// !! This stopped working after the great autorecord/sync 
		// rewrite.  Scripts are expecting samples to play into the input 
		// buffer immediately, at least after a Wait has been executed
		// and we're out of latency compensation mode.  We probably need to 
		// be more careful about passing the latency context down
		// from SampleManager::trigger, until then assume we're not
		// compensating for latency

        //mFrame = -(mSample->mInputLatency);
		mFrame = 0;
    }

}

bool SampleCursor::isStopping()
{
    return mStop;
}

bool SampleCursor::isStopped()
{
    bool stopped = mStopped;
    
    // if we're a play cursor, then we're not considered stopped
    // until the record cursor is also stopped
    if (mRecord != nullptr)
      stopped = mRecord->isStopped();

    return stopped;
}

/**
 * Called when we're supposed to stop the cursor.
 * We'll continue on for a little while longer so we can fade
 * out smoothly.  This is called only for the play cursor,
 * the record cursor lags behind so we call stop(frame) when
 * we know the play frame to stop on.
 * 
 */
void SampleCursor::stop()
{
    if (!mStop) {
		long maxFrames = 0;
        Audio* audio = mSample->getAudio();
		long sampleFrames = audio->getFrames();
		maxFrames = mFrame + AudioFade::getRange();
		if (maxFrames >= sampleFrames) {
			// must play to the end assume it has been trimmed
			// !! what about mLoop, should we set this
			// to sampleFrames so it can end?
			maxFrames = 0;
		}

		stop(maxFrames);
		if (mRecord != nullptr)
		  mRecord->stop(maxFrames);
	}
}

/**
 * Called for both the play and record cursors to stop on a given
 * frame.  If the frame is before the end of the audio, then we set
 * up a fade.
 */
void SampleCursor::stop(long maxFrames)
{
    if (!mStop) {
        // old code had this but was unreferenced
        //Audio* audio = mSample->getAudio();
		if (maxFrames > 0)
		  mAudioCursor->setFadeOut(maxFrames);
		mMaxFrames = maxFrames;
        mStop = true;
    }
}

/**
 * Play/Record more frames in the sample.
 */
void SampleCursor::play(float* inbuf, float* outbuf, long frames)
{
	// play
	if (outbuf != nullptr)
	  play(outbuf, frames);

	// record
	if (mRecord != nullptr && inbuf != nullptr)
	  mRecord->play(inbuf, frames);
}

/**
 * Play more frames in the sample.
 */
void SampleCursor::play(float* outbuf, long frames)
{
    Audio* audio = mSample->getAudio();
    if (audio != nullptr && !mStopped) {

        // consume dead input latency frames in record cursors
        if (mFrame < 0) {
            mFrame += frames;
            if (mFrame > 0) {
                // we advanced into "real" frames, back up
                int ignored = (int)(frames - mFrame);
                outbuf += (ignored * audio->getChannels());
                frames = mFrame;
                mFrame = 0;
            }
            else {
                // nothing of interest for this buffer
                frames = 0;
            }
        }

        // now record if there is anything left in the buffer
        if (frames > 0) {

			// !! awkward interface
			AudioBuffer b;
			b.buffer = outbuf;
			b.frames = frames;
			b.channels = 2;
			mAudioCursor->setAudio(audio);
			mAudioCursor->setFrame(mFrame);

            long sampleFrames = audio->getFrames();
            if (mMaxFrames > 0)
              sampleFrames = mMaxFrames;
            
            long lastBufferFrame = mFrame + frames - 1;
            if (lastBufferFrame < sampleFrames) {
				mAudioCursor->get(&b);
                mFrame += frames;
            }
            else {
                long avail = sampleFrames - mFrame;
                if (avail > 0) {
					b.frames = avail;
					mAudioCursor->get(&b);
                    mFrame += avail;
                }

                // if we get to the end of a sustained sample, and the
                // trigger is still down, loop again even if the loop 
                // option isn't on

                if (!mSample->mLoop &&
                    !(mSample->mDown && mSample->mSustain)) {
                    // we're done
                    mStopped = true;
                }
                else {
                    // loop back to the beginning
                    long remainder = frames - avail;
                    outbuf += (avail * audio->getChannels());

                    // should already be zero since if we ended a sustained
                    // sample early, it would have been handled in stop()?
                    if (mMaxFrames > 0)
                      Trace(1, "SampleCursor::play unexpected maxFrames\n");
                    mMaxFrames = 0;
                    mFrame = 0;

                    sampleFrames = audio->getFrames();
                    if (sampleFrames < remainder) {
                        // sample is less than the buffer size?
                        // shouldn't happen, handling this would make this
                        // much more complicated, we'd have to loop until
                        // the buffer was full
                        remainder = sampleFrames;
                    }

					b.buffer = outbuf;
					b.frames = remainder;
					mAudioCursor->setFrame(mFrame);
					mAudioCursor->get(&b);
                    mFrame += remainder;
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// SampleManager
//
//////////////////////////////////////////////////////////////////////

/**
 * Don't need this.  mSampleCount was calculated at construction and you
 * can't change the list after that.
 */
int SampleManager::getSampleCount()
{
    int count = 0;
    for (SamplePlayer* p = mPlayerList ; p != nullptr ; p = p->getNext())
      count++;
    return count;
}

/**
 * Called whenever a new MobiusConfig is installed.  Check for changes
 * in latency for compensation.
 *
 * TOOD: formerly got this from MobiusConfig which tried to force the
 * buffer size, and allowed percieved latency to be overridden from what
 * the hardware things for testing and performance tuning.
 *
 * Probably still want an override but now these should default to
 * comming from the AudioStream.
 */
void SampleManager::updateConfiguration(MobiusConfig* config)
{
    // commented out till we can work through the new MobiusContainer
    (void)config;
#if 0
    // config is ignored since we're only interested in latencies right now
	for (int i = 0 ; i < mSampleCount ; i++)
	  mPlayers[i]->updateConfiguration(mMobius->getEffectiveInputLatency(),
									   mMobius->getEffectiveOutputLatency());
#endif
}

/**
 * Trigger a sample to begin playing. Called by the SamplePlay action.
 * This most often comes from a test script.
 *
 * Note that this fills BOTH the input and output buffers, though
 * technically a sample "player" should fill only the output buffer.
 * This is because test scripts want to have tracks recording
 * the samples being played, which is what they were originally designed for.
 *
 * If we evolve this to be more of a pure player may want control over that.
 *
 * This is assuming that samples can only be trigger at the start
 * of an interrupt block so we are allowed to fill the entire interrupt
 * buffer.  If we ever get to a point where sample triggers can
 * be quantized or stacked on other events, then this will need more
 * coordination with the Track timeline so we know the offset into the
 * current buffer to begin depositing content.
 *
 * new: see notes/samples-block-size.txt for why this was done wrong
 * before, and was still done wrong in the initial port, and why unit tests
 * mess up if the block size isn't 256.
 *
 * The blockOffset is the location within the current interrupt block where the
 * sample playback logically starts injecting.
 *
 * The sampleOffset is the location within the triggered sample to start playing.
 * This is only set when trying to compensate for old unit test block sizes.
 *
 * Ugh, each SamplePlayer can have multiple cursors representing previous
 * triggers of this sample, This needs to be redesigned so that here we can
 * "Play" all the active cursors but just do the special offset processing
 * on the new cursor.  
 * 
 */
float* SampleManager::trigger(MobiusAudioStream* stream, int index, bool down)
{
    float* modified = nullptr;
    
	if (index < mSampleCount) {
		mPlayers[index]->trigger(down);
		mLastSample = index;
        
        long frames = stream->getInterruptFrames();
        float* input = nullptr;
        float* output = nullptr;

        stream->getInterruptBuffers(0, &input, 0, &output);
#if 0
        if (blockOffset > frames) {
            // calculation error
            Trace(1, "SampleManager::trigger block offset overflow %ld", (long)blockOffset);
            blockOffset = 0;
        }
        else {
            int samples = blockOffset * 2;
            input += samples;
            output += samples;
            frames -= blockOffset;
        }
#endif        

        if (frames > 0) {
            mPlayers[index]->play(input, output, frames);
            // tell Kernel which one we modified so it can notify Tracks
            modified = input;
        }
	}
	else {
		// this is sometimes caused by a misconfiguration of the
		// the unit tests
		Trace(1, "ERROR: No sample at index %ld\n", (long)index);
	}

    return modified;
}

/**
 * Alternative trigger when the action is directly associated
 * with a Symbol that has a SamplePlayer.  Since mLastSample needs
 * the index, calculate it and call the other trigger method.
 */
float* SampleManager::trigger(MobiusAudioStream* stream, SamplePlayer* player, bool down)
{
    int index = 0;
    bool found = false;

    for (int i = 0 ; i < mSampleCount ; i++) {
        SamplePlayer* p = mPlayers[i];
        if (p == player) {
            index = i;
            found = true;
            break;
        }
    }

    return trigger(stream, index, down);
}

long SampleManager::getLastSampleFrames()
{
	long frames = 0;
	if (mLastSample >= 0)
	  frames = mPlayers[mLastSample]->getFrames();
	return frames;
}

//////////////////////////////////////////////////////////////////////
//
// Interrupt Handler
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by MobiusKernel when buffers are received from the container.
 * 
 * In the old Recorder model, each track could be configured to receive/send
 * on a "port" which was a set of stereo channels.  Samples were always processed
 * on port zero, which was fine for testing but would need to be more flexible
 * if this ever evolves.
 *
 * Note that if samples are triggered during this interrupt we'll
 * end up in trigger() above which will start another play cursor
 * and add even more content to the buffers.
 */
void SampleManager::processAudioStream(MobiusAudioStream* stream)
{
    long frames = stream->getInterruptFrames();
    float* input = nullptr;
    float* output = nullptr;

    stream->getInterruptBuffers(0, &input, 0, &output);

	for (int i = 0 ; i < mSampleCount ; i++)
	  mPlayers[i]->play(input, output, frames);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
