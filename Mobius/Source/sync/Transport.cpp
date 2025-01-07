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

/**
 * The minimum allowable unit length.
 * This should be around the length of one block.
 * Mostly it just needs to be above zero.
 */
const int TransportMinLength = 128;

Transport::Transport(SyncMaster* sm)
{
    syncMaster = sm;
    // this will usually be wrong, setSampleRate needs to be called
    // after the audio stream is initialized to get the right one
    sampleRate = 44100;

    // initial time signature
    state.unitsPerBeat = 1;
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
 * active tracks going and the unitLength changes, all sorts of weird things can
 * happen.
 */
void Transport::setSampleRate(int rate)
{
    sampleRate = rate;
    setTempo(state.tempo);
}

/**
 * Set the tempo, typically specified by the user.
 * This results in a recalculation of the unit lengths.
 * This is an alternative to setLength() which calculates the
 * tempo from the length.
 */
void Transport::setTempo(float tempo)
{
    deriveUnitLength(tempo);
}

/**
 * Set the length of the transport loop, typically after recording the first loop.
 * This results in a recalculation of the tempo.
 *
 * This is more complicated than setting a user-specified tempo
 * because the resulting tempo may either be unusably fast or slow, so the
 * tempo may be adjusted.
 */
int Transport::setLength(int length)
{
    return deriveTempo(length);
}

/**
 * Changing the beats per bar does not change the unit length length or the
 * tempo but it will change where beat pulses are.
 *
 * It will factor into future calculations if the tempo or length is changed.
 * !! is this enough, feels like there should be more here.
 *
 * At the very least, we should recalculate all the derived bat/bar/loop counters
 */
void Transport::setBeatsPerBar(int bpb)
{
    if (bpb > 0) {
        state.beatsPerBar = bpb;
    }
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

void Transport::refreshState(SyncSourceState& dest)
{
    dest = state;
}

/**
 * Capture the priority state from the transport.
 */
void Transport::refreshPriorityState(PriorityState* dest)
{
    dest->transportBeat = state.beat;
    dest->transportBar = state.bar;
    dest->transportLoop = state.loop;
}

float Transport::getTempo()
{
    return state.tempo;
}

int Transport::getBeatsPerBar()
{
    return state.beatsPerBar;
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
// Internal Transport Controls
//
//////////////////////////////////////////////////////////////////////

/**
 * does this "resume" or start from zero?
 */
void Transport::start()
{
    state.started = true;
}

void Transport::startClocks()
{
}

void Transport::stop()
{
    state.started = false;
    state.playFrame = 0;
    state.units = 0;
    state.beat = 0;
    state.bar = 0;
    state.loop = 0;
    state.songClock = 0;
}

void Transport::stopSelective(bool sendStop, bool stopClocks)
{
    (void)sendStop;
    (void)stopClocks;
}

void Transport::midiContinue()
{
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

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Advance the transport and detect whether a pulse was encountered.
 */
void Transport::advance(int frames)
{
    pulse.reset();

    if (state.started) {

        state.playFrame = state.playFrame + frames;
        if (state.playFrame >= state.unitFrames) {

            // a unit has transpired
            int blockOffset = state.unitFrames - state.playFrame;
            if (blockOffset > frames)
              Trace(1, "Transport: You suck at math");

            state.playFrame = state.playFrame - state.unitFrames;

            state.units++;
            state.unitCounter++;

            if (state.unitCounter >= state.unitsPerBeat) {

                state.unitCounter = 0;
                state.beat++;

                if (state.beat >= state.beatsPerBar) {

                    state.beat = 0;
                    state.bar++;

                    if (state.bar >= state.barsPerLoop) {

                        state.bar = 0;
                        state.loop++;

                        pulse.type = Pulse::PulseLoop;
                    }
                    else {
                        pulse.type = Pulse::PulseBar;
                    }
                }
                else {
                    pulse.type = Pulse::PulseBeat;
                }
                
                pulse.blockFrame = blockOffset;
            }
        }
    }
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

void Transport::correctBaseCounters()
{
    if (state.unitsPerBeat < 1) state.unitsPerBeat = 1;
    if (state.beatsPerBar < 1) state.beatsPerBar = 1;
    if (state.barsPerLoop < 1) state.barsPerLoop = 1;
}

/**
 * Calculate the tempo based on a frame length from the outside.
 *
 * There are two outcomes, depending on options: constrained tempo
 * and locked bars.
 *
 * With constrained tempo, the tempo will be kept within the configured minimum
 * and maximum range, adjusting the number of bars within the length.  This may mean
 * that in order to keep the tempo within range you end up with an odd number of bars.
 *
 * With locked bars, the beatsPerBar and barsPerLoop are held constant and the tempo
 * is calculated for that and may vary widely.  The bar count is allowed to double or halve
 * to bring the tempo into a more usable range but this will not result in an odd multiple
 * of barsPerLoop.  This method would be used if you were generating clocks for an external
 * sequencer such as a drum machine that is playing a sequence of known length.
 */
int Transport::deriveTempo(int length)
{
    // todo, if we need to round the unit length this would be left here
    // and returned so the caller can add or remove this amount from the recorded loop
    int adjust = 0;
    int oldUnit = state.unitFrames;
    
    // todo: would we allow setting length to zero to reset the transport?
    if (length < TransportMinLength) {
        Trace(1, "Transport: Length out of range %d", length);
    }
    else {
        // correct these before we start dividing
        correctBaseCounters();

        int barCount = state.barsPerLoop;
        float tempo = lengthToTempo(length, barCount, state.beatsPerBar);

        if (barLock) {
            // the specified number of bars may not be changed
            // the tempo is what it is
        }
        else if (barMultiply) {
            // the bars may double or halve to bring the tempo in range
            if (tempo < minTempo) {
                while (tempo < minTempo) {
                    barCount *= 2;
                    tempo = lengthToTempo(length, barCount, state.beatsPerBar);
                    // todo: should have a governor on the number of bars?
                }
            }
            else if (tempo > maxTempo) {
                // can only do this if the bar count is even
                if ((barCount % 2) > 0) {
                    Trace(2, "Transport: Unable to divide uneven bar count %d", barCount);
                }
                else {
                    while (tempo > maxTempo && barCount > 1) {
                        barCount /= 2;
                        tempo = lengthToTempo(length, barCount, state.beatsPerBar);
                    }
                }
            }
        }
        else {
            // bar count may be freely adjusted to keep the tempo in range
            if (tempo < minTempo) {
                // math: be better
                while (tempo < minTempo) {
                    barCount++;
                    tempo = lengthToTempo(length, barCount, state.beatsPerBar);
                    // todo: should have a governor on the number of bars?
                }
            }
            else if (tempo > maxTempo) {
                while (tempo > maxTempo && barCount > 1) {
                    barCount--;
                    tempo = lengthToTempo(length, barCount, state.beatsPerBar);
                }
            }
        }

        // now work backwards to get ingegral frame lengths and calculate
        // the adjustment
        int barFrames = length / barCount;
        int beatFrames = barFrames / state.beatsPerBar;

        state.unitFrames = beatFrames;
        state.unitsPerBeat = 1;

        barFrames = beatFrames * state.beatsPerBar;
        int loopFrames = barFrames * barCount;

        if (loopFrames != length) {
            adjust = loopFrames - length;
            Trace(2, "Transport: Adjusted loop frames from %d to %d",
                  length, loopFrames);
        }
                    
        state.tempo = tempo;
        state.barsPerLoop = barCount;
	}

    deriveLocation(oldUnit);

    return adjust;
}

float Transport::lengthToTempo(int length, int barCount, int beatsPerBar)
{
    float barFrames = (float)length / (float)barCount;
    float framesPerBeat = barFrames / (float)beatsPerBar;
    float secondsPerBeat = framesPerBeat / (float)sampleRate;
    float tempo = 60.0f / secondsPerBeat;

    return tempo;
}

/**
 * Given the desired tempo, determine the unit lengths.
 * The tempo may be adjusted slightly to allow for integral unitFrames.
 */
void Transport::deriveUnitLength(float tempo)
{
    int oldUnit = state.unitFrames;
    
    if (tempo < TransportMinTempo)
      tempo = TransportMinTempo;
    else if (tempo > TransportMaxTempo)
      tempo = TransportMaxTempo;

    correctBaseCounters();

    float beatsPerSecond = tempo / 60.0f;
    int framesPerBeat = (int)((float)sampleRate / beatsPerSecond);

    state.tempo = tempo;
    state.unitFrames = framesPerBeat;
    state.unitsPerBeat = 1;

    deriveLocation(oldUnit);
}

/**
 * After deriving either the tempo or the length wrap the playFrame
 * if necessary and reorient the counters.
 */
void Transport::deriveLocation(int oldUnit)
{
    if (state.unitFrames <= 0) {
        Trace(1, "Transport: Wrap with empty unit frames");
    }
    else {
        // playFrame must always be within the unit length
        while (state.playFrame > state.unitFrames)
          state.playFrame -= state.unitFrames;

        // if we changed the unitLength, then the beat/bar/loop
        // boundaries have also changed and the current boundary counts
        // may no longer be correct
        if (oldUnit != state.unitFrames) {

            // I'm tired, punt and put them back to zero
            
            // determine the absolute position using the old unit
            int oldBeatFrames = oldUnit * state.unitsPerBeat;
            int oldBarFrames = oldBeatFrames * state.beatsPerBar;
            int oldLoopFrames = oldBarFrames * state.barsPerLoop;
            int oldAbsoluteFrame = oldLoopFrames * state.loop;

            // now do a bunch of divides to work backward from oldAbsoluteFrame
            // using the new unit length
            (void)oldBeatFrames;
            (void)oldBarFrames;
            (void)oldLoopFrames;
            (void)oldAbsoluteFrame;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

