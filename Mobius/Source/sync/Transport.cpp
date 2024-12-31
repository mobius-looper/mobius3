/**
 * Notes on time:
 *
 * 44100    samples (frames) per second
 * 44.10	samples per millisecond
 * .02268	milliseconds per sample
 * 256  	frames per block
 * 5.805	milliseconds per block
 * 172.27	blocks per second
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/PriorityState.h"

#include "Pulse.h"
#include "Transport.h"

/**
 * The maximum allowed tempo.
 * As the tempo increases, the beat length decreases.
 * 
 * The only hard constraint we have here is that the tempo can't be
 * so fast that it would result in more than one beat pulse per audio
 * block since Pulsator doesn't handle that.
 *
 * With a 44100 rate and 256 blocks, that's 172 blocks per second.
 * One beat per block would be the equivalent of a BPM of 10,320.
 * 
 * This can be configured lower by the user but not higher.
 */
const float TransportMaxTempo = 1000.0f;

/**
 * The minimum tempo needs more thought.
 * As the tempo decreases, the beat length increases.
 * 
 * It would be nice to allow a tempo of zero to be allowed which has
 * the effect of stopping the transport.  But that doesn't mean the
 * loop is infinitely long.  It's rather an adjustment to the playback
 * rate of that loop.
 *
 * A tempo of 10 with a sample rate of 44100 results in a beat length
 * of 264,705 frames.
 */
const float TransportMinTempo = 10.0f;

Transport::Transport()
{
    sampleRate = 44100;
    beatsPerBar = 4;
    setTempo(120.0f);
}

Transport::~Transport()
{
}

/**
 * The audio stream sample rate is necessary for calculating
 * tempo and timeline frames.  It generally does not change.
 *
 * todo: needs some sanity checks on the range
 */
void Transport::setSampleRate(int r)
{
    sampleRate = r;
    // go through tempo/length calculations again
    // this is okay if we're being initialized, but if you change
    // the sample rate while a session is active this changes the sync master
    // length and all sorts of things are going to go haywire
    setTempo(tempo);
}

float Transport::getTempo()
{
    return tempo;
}

int Transport::getMasterBarFrames()
{
    return masterBarFrames;
}

int Transport::getBeatsPerBar()
{
    return beatsPerBar;
}

/**
 * Set the tempo, typically specified by the user.
 * This results in a recalculation of the timeline length.
 * This becomes the "master sync unit".
 */
void Transport::setTempo(float t)
{
    if (t < TransportMinTempo)
      t = TransportMinTempo;
    else if (t > TransportMaxTempo)
      t = TransportMaxTempo;
    
    tempo = t;
    masterBarFrames = 0;
    framesPerBeat = 0;

    // correct this if unset
    if (beatsPerBar < 1) beatsPerBar = 1;

    float beatsPerSecond = tempo / 60.0f;
    framesPerBeat = (int)((float)sampleRate / beatsPerSecond);
    int framesPerBar = framesPerBeat * beatsPerBar;
    // not supporting multiple bars yet
    masterBarFrames = framesPerBar;

    // if we were currently playing, this may have made it shorter
    wrap();
}

/**
 * Set the length of the timeline, typically after recording the first loop.
 * This results in a recalculation of the tempo.
 *
 * This is more complicated than setting a user-specified tempo
 * because the resulting tempo may either be unusably fast or slow, so the
 * tempo may be adjusted.
 */
void Transport::setMasterBarFrames(int f)
{
    if (f > 0) {
        masterBarFrames = f;
        recalculateTempo();
    }
}

/**
 * Changing the beats per bar does not change the timeline length or the
 * tempo but it will change where beat pulses are.
 *
 * todo: need to have a bounds check here so there can't be more than
 * one beat pulse per block.  bpb would have to be extremely large for
 * that to be hit.
 */
void Transport::setBeatsPerBar(int bpb)
{
    if (bpb > 0) {
        beatsPerBar = bpb;
        // this can result in roundoff error where framesPerBeat * beatsPerBar
        // is not exactly equal to masterBarFrames
        // that's okay as long as the final pulse uses masterBarFrames to trigger
        // rather than inner beat calculations
        framesPerBeat = masterBarFrames / beatsPerBar;
    }
}

void Transport::setTimelineFrame(int f)
{
    if (f >= 0) {
        playFrame = f;
        wrap();
    }
}

void Transport::wrap()
{
    if (masterBarFrames == 0) {
        playFrame = 0;
    }
    else {
        // hey, can't math do this?
        while (playFrame > masterBarFrames)
          playFrame -= masterBarFrames;
    }
}

int Transport::getTimelineFrame()
{
    return playFrame;
}

/**
 * Capture the priority state from the transport.
 */
void Transport::refreshPriorityState(PriorityState* state)
{   
    state->transportBar = barHit;
    barHit = false;
    state->transportBeat = beatHit;
    beatHit = false;
}

//////////////////////////////////////////////////////////////////////
//
// Traansport Controls
//
//////////////////////////////////////////////////////////////////////

void Transport::start()
{
    started = true;
}

void Transport::stop()
{
    started = false;
    playFrame = 0;
}

void Transport::pause()
{
    paused = true;
}

bool Transport::isStarted()
{
    return started;
}

bool Transport::isPaused()
{
    return paused;
}

int Transport::getBeat()
{
    return beat;
}

int Transport::getBar()
{
    return bar;
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance the transport and let the caller (SyncMaster) know if
 * it crossed a beat or bar boundary.
 */
bool Transport::advance(int frames, Pulse& p)
{
    bool pulseEncountered = false;

    if (started) {
        if (playFrame >= masterBarFrames) {
            // bad dog, could wrap here but shouldn't be happening
            Trace(1, "Transport: Play frame beyond the end");
        }
        else {
            int newPlayFrame = playFrame + frames;
            if (newPlayFrame >= masterBarFrames) {
                // todo: multiple bars and PulseLoop?
                pulseEncountered = true;
                p.type = Pulse::PulseBar;
                p.blockFrame = masterBarFrames - playFrame;
                newPlayFrame -= masterBarFrames;
                if (newPlayFrame > masterBarFrames) {
                    // something is off here, must be an extremely short block size
                    Trace(1, "Transport: PlayFrame anomoly");
                }
                playFrame = newPlayFrame;
                beat = 0;
                // for state refresh
                barHit = true;
            }
            else if (framesPerBeat > 0) {
                int newBeat = newPlayFrame / framesPerBeat;
                if (newBeat != beat) {
                    pulseEncountered = true;
                    p.type = Pulse::PulseBeat;
                    int beatBase = newBeat * framesPerBeat;
                    int beatOver = newPlayFrame - beatBase;
                    p.blockFrame = frames - beatOver;
                    beat = newBeat;
                    beatHit = true;
                }
            }
        }
    }
    return pulseEncountered;
}

//////////////////////////////////////////////////////////////////////
//
// Tempo Math
//
//////////////////////////////////////////////////////////////////////

/**
 * Calculate the tempo based on a frame length from the outside.
 *
 * todo: this needs more extreme range checking because we can get
 * 32-bit float errors deriving the tempo from the length which can cause
 * drift.  It would be better to *2 or /2 the length to make the tempo fit
 * rather than the other way around?  I guess it's the same drift.
 *
 */
void Transport::recalculateTempo()
{
    if (masterBarFrames == 0) {
        // degenerate case, not sure what this means
        Trace(1, "Transport::recalculateTempo Timeline length is zero");
    }
    else {
        // correct this if it isn't by now
        if (beatsPerBar < 1) beatsPerBar = 1;
    
        float floatFramesPerBeat = (float)masterBarFrames / (float)beatsPerBar;
        float secondsPerBeat = floatFramesPerBeat / (float)sampleRate;
        float newTempo = 60.0f / secondsPerBeat;

        // todo: need user configurable min/max tempo
        float minTempo = 30.0f;
        float maxTempo = 400.0f;
        
		while (newTempo > maxTempo) {
			newTempo /= 2.0;
		}

		// if a conflicting min/max specified, min wins
		while (newTempo < minTempo)
          newTempo *= 2.0;

        tempo = newTempo;

        float beatsPerSecond = tempo / 60.0f;
        framesPerBeat = (int)((float)sampleRate / beatsPerSecond);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
