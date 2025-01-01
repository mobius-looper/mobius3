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
#include "SyncSourceState.h"
#include "SyncMaster.h"
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

Transport::Transport(SyncMaster* sm))
{
    syncMaster = sm;
    // this will usually be wrong, setSampleRate needs to be called
    // after the audio stream is initialized to get the right one
    sampleRate = 44100;
    state.beatsPerBar = 4;
    setTempo(120.0f);
}

Transport::~Transport()
{
}

/**
 * Called whenever the sample rate changes.
 * Initialization happens before the audio devices are open so MobiusContainer
 * won't have the right one when we were constructured.  It may also change at any
 * time after initialization if the user fiddles with the audio device configuration.
 *
 * Since this is used for tempo calculations, go through the tempo/length calculations
 * whenever this changes.  This is okay when the system is quiet, but if there are
 * active tracks going and the masterBarLength changes, all sorts of weird things can
 * happen.
 */
void Transport::setSampleRate(int r)
{
    sampleRate = r;
    setTempo(tempo);
}

float Transport::getTempo()
{
    return tempo;
}

SyncSourceState* getState()
{
    return &state;
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
    
    state.tempo = t;
    state.masterBarFrames = 0;
    framesPerBeat = 0;

    // correct this if unset
    if (state.beatsPerBar < 1) state.beatsPerBar = 1;

    float beatsPerSecond = state.tempo / 60.0f;
    framesPerBeat = (int)((float)sampleRate / beatsPerSecond);
    int framesPerBar = framesPerBeat * beatsPerBar;
    // not supporting multiple bars yet
    state.barFrames = framesPerBar;

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
        state.barFrames = f;
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
        state.beatsPerBar = bpb;
        // this can result in roundoff error where framesPerBeat * beatsPerBar
        // is not exactly equal to masterBarFrames
        // that's okay as long as the final pulse uses masterBarFrames to trigger
        // rather than inner beat calculations
        framesPerBeat = state.barFrames / state.beatsPerBar;
    }
}

void Transport::setTimelineFrame(int f)
{
    if (f >= 0) {
        state.playFrame = f;
        wrap();
    }
}

void Transport::wrap()
{
    if (state.barFrames == 0) {
        state.playFrame = 0;
    }
    else {
        // hey, can't math do this?
        while (state.playFrame > state.barFrames)
          state.playFrame -= state.barFrames;
    }
}

int Transport::getTimelineFrame()
{
    return state.playFrame;
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
// Internal Transport Controls
//
//////////////////////////////////////////////////////////////////////

void Transport::start()
{
    state.started = true;
}

void Transport::stop()
{
    state.started = false;
    state.playFrame = 0;
}

void Transport::pause()
{
    paused = true;
}

bool Transport::isStarted()
{
    return state.started;
}

bool Transport::isPaused()
{
    return paused;
}

int Transport::getBeat()
{
    return state.beat;
}

int Transport::getBar()
{
    return state.bar;
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance the transport and detect whether a pulse was encountered.
 */
bool Transport::advance(int frames)
{
    bool pulseEncountered = false;
    pulse.reset();

    if (state.started) {
        int masterBarFrames = state.barFrames;
        
        if (state.playFrame >= masterBarFrames) {
            // bad dog, could wrap here but shouldn't be happening
            Trace(1, "Transport: Play frame beyond the end");
        }
        else {
            int newPlayFrame = state.playFrame + frames;
            if (newPlayFrame >= masterBarFrames) {
                // todo: multiple bars and PulseLoop?
                pulseEncountered = true;
                pulse.type = Pulse::PulseBar;
                pulse.blockFrame = masterBarFrames - state.playFrame;
                newPlayFrame -= masterBarFrames;
                if (newPlayFrame > masterBarFrames) {
                    // something is off here, must be an extremely short block size
                    Trace(1, "Transport: PlayFrame anomoly");
                }
                state.playFrame = newPlayFrame;
                state.beat = 0;
                // for state refresh
                barHit = true;
            }
            else if (framesPerBeat > 0) {
                int newBeat = newPlayFrame / framesPerBeat;
                if (newBeat != beat) {
                    pulseEncountered = true;
                    pulse.type = Pulse::PulseBeat;
                    int beatBase = newBeat * framesPerBeat;
                    int beatOver = newPlayFrame - beatBase;
                    pulse.blockFrame = frames - beatOver;
                    beat = newBeat;
                    beatHit = true;
                }
            }
        }
    }
    return pulseEncountered;
}

Pulse* Transport::getPulse()
{
    return &pulse;
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
    if (state.barFrames == 0) {
        // degenerate case, not sure what this means
        Trace(1, "Transport::recalculateTempo Timeline length is zero");
    }
    else {
        // correct this if it isn't by now
        if (state.beatsPerBar < 1) state.beatsPerBar = 1;
    
        float floatFramesPerBeat = (float)(state.barFrames) / (float)(state.beatsPerBar);
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

        state.tempo = newTempo;

        float beatsPerSecond = newTempo / 60.0f;
        framesPerBeat = (int)((float)sampleRate / beatsPerSecond);
	}
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
