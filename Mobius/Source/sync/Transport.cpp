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
#include "../model/Session.h"

#include "Pulse.h"
#include "SyncSourceState.h"
#include "SyncMaster.h"
#include "MidiRealizer.h"
#include "Pulsator.h"
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
 * The minimum allowable unit length in frames.
 * This should be around the length of one block.
 * Mostly it just needs to be above zero.
 */
const int TransportMinFrames = 128;

Transport::Transport(SyncMaster* sm)
{
    syncMaster = sm;
    midiRealizer = sm->getMidiRealizer();
    // this will usually be wrong, setSampleRate needs to be called
    // after the audio stream is initialized to get the right one
    sampleRate = 44100;

    // initial time signature
    state.unitsPerBeat = 1;
    state.beatsPerBar = 4;

    setTempo(90.0f);

    //midiRealizer->setTraceEnabled(true);
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

void Transport::loadSession(Session* s)
{
    midiEnabled = s->getBool(MidiEnable);
    sendClocksWhenStopped = s->getBool(ClocksWhenStopped);
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

void Transport::resetLocation()
{
    state.playFrame = 0;
    state.units = 0;
    state.beat = 0;
    state.bar = 0;
    state.loop = 0;
    state.songClock = 0;
}

/**
 * does this "resume" or start from zero?
 */
void Transport::start()
{
    state.started = true;

    // going to need a lot more state here
    if (midiEnabled)
      midiRealizer->start();
    
}

void Transport::startClocks()
{
    if (midiEnabled)
      midiRealizer->startClocks();
}

void Transport::stop()
{
    if (midiEnabled) {
        if (sendClocksWhenStopped)
          midiRealizer->stopSelective(true, false);
        else
          midiRealizer->stop();
    }
    
    state.started = false;
    resetLocation();
}

void Transport::stopSelective(bool sendStop, bool stopClocks)
{
    Trace(1, "Transport::stopSelective");
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
            int blockOffset = state.playFrame - state.unitFrames;
            if (blockOffset > frames || blockOffset < 0)
              Trace(1, "Transport: You suck at math");

            // effectively a frame wrap too
            state.playFrame = blockOffset;

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
 * Struggling with options here, but need to guess the user's intent for
 * the length.  The most common use for this is tap tempo, where each tap
 * length represents one beat which becomes the unit length.
 *
 * But they could also be thinking of tapping bars, where the tep length would be divided
 * by beatsPerBar to derive the unit length.
 *
 * Or they could be tapping an entire loop dividied by barsPerLoop (e.g. 12-bar pattern)
 * and beatsPerBar.
 *
 * Without guidance, we would need to guess by seeing which lenght assumption results
 * in a tempo that is closest with the fewest adjustments.
 *
 * Start with simple tempo double/halve and revisit this.
 */
int Transport::deriveTempo(int tapFrames)
{
    int adjust = 0;

    // todo: would we allow setting length to zero to reset the transport?
    if (tapFrames < TransportMinFrames) {
        Trace(1, "Transport: Tap frames out of range %d", tapFrames);
    }
    else {
        int unitFrames = tapFrames;
        float tempo = lengthToTempo(unitFrames);

        if (tempo > maxTempo) {
            while (tempo > maxTempo) {
                unitFrames *= 2;
                tempo = lengthToTempo(unitFrames);
            }
        }
        else if (tempo < minTempo) {

            if ((unitFrames % 2) > 0) {
                Trace(2, "Transport: Rounding odd unit length %d", unitFrames);
                unitFrames--;
                adjust = unitFrames - tapFrames;
            }
            
            while (tempo < minTempo) {
                unitFrames /= 2;
                tempo = lengthToTempo(unitFrames);
            }
        }

        state.unitFrames = unitFrames;
        state.unitsPerBeat = 1;
        setTempoInternal(tempo);
	}

    //deriveLocation(oldUnit);
    resetLocation();

    return adjust;
}

void Transport::setTempoInternal(float tempo)
{
    state.tempo = tempo;
    midiRealizer->setTempo(tempo);
    if (midiEnabled && sendClocksWhenStopped)
      midiRealizer->startClocks();
}

float Transport::lengthToTempo(int frames)
{
    float secondsPerUnit = (float)frames / (float)sampleRate;
    float tempo = 60.0f / secondsPerUnit;
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

    state.unitFrames = framesPerBeat;
    state.unitsPerBeat = 1;
    setTempoInternal(tempo);

    //deriveLocation(oldUnit);
    (void)oldUnit;
    resetLocation();
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

//////////////////////////////////////////////////////////////////////
//
// Drift Checking
//
//////////////////////////////////////////////////////////////////////

/**
 * The relationship between Transport and Pulsator is awkward now that
 * Transport controls MidiRealizer but Pulsator consumes the MidiQueue event
 * list and calculates pulses.  Transport block processing needs to be done
 * in two phases.  First advance() is called so Transport can analyze it's own
 * pulses to be provided to Pulsator.
 *
 * After Pulsator finishes, Transporter gets control again to look at the pulses
 * generated by MidiRealizer to check drift.  Since nothing is supposed to be using
 * MidiRealizer directly for pulses, it would be more self contained if Transport
 * did the MidiQueue analysis rather than Pulsator.  What has to happen is the same
 * it's just organized better.
 *
 * Pulsator doesn't deal with all of the various realtime events that MidiRealizer
 * can convey, this could be a problem here drift checking, might need to go against
 * MidiRealizer directly to get songClocks and things.
 */
void Transport::checkDrift()
{
    if (state.started) {
        Pulsator* pulsator = syncMaster->getPulsator();
        Pulse* p = pulsator->getOutBlockPulse();
        if (p != nullptr) {
            if (p->type == Pulse::PulseBeat) {
                Trace(2, "Transport: Beat");
            }
            else {
                Trace(2, "Transport: Bar");
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

