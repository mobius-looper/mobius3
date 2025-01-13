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
#include "../model/Session.h"
#include "../model/PriorityState.h"
#include "../mobius/track/TrackProperties.h"

#include "SyncConstants.h"
#include "SyncSourceState.h"
#include "SyncMaster.h"
#include "MidiRealizer.h"
#include "Pulsator.h"

#include "Transport.h"

//////////////////////////////////////////////////////////////////////
//
// Limits
//
//////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////
//
// Initialization
//
//////////////////////////////////////////////////////////////////////

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

    userSetTempo(90.0f);

    //midiRealizer->setTraceEnabled(true);

    testCorrection = false;
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
    drifter.setSampleRate(rate);
    
    // not a user action, but sort of is because they manually changed
    // the audio interface, might need to streamline the process here
    userSetTempo(state.tempo);
}

void Transport::loadSession(Session* s)
{
    midiEnabled = s->getBool(MidiEnable);
    sendClocksWhenStopped = s->getBool(ClocksWhenStopped);

    if (!midiEnabled) {
        midiRealizer->stop();
    }
    else if (sendClocksWhenStopped) {
        if (!state.started)
          midiRealizer->startClocks();
    }
    else {
        if (!state.started)
          midiRealizer->stopSelective(false, true);
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
// Manual Control
//
// These are underneath action handlers sent by the UI and provide
// transport control directly to the user rather than automated control
// that happens from within by restructuring around tracks.
//
// The UI may choose to prevent manual control when there is currentl
// a track connected to the transport.  User commands that change the
// tempo/unit length effectively break the connection between the transport
// and the track.
//
//////////////////////////////////////////////////////////////////////

/**
 * The user has pressed a "Start" button or taken some other action that
 * is expected to start the transport.
 *
 * If the transport is already started, nothing changes.
 * If the transport is connected to a track and paused, then it will
 * be forcibly resumed and may drift apart from the track.
 *
 * todo: once we allow this, then will probably want various forms of
 * realign to either bring the track into alignment with the transport
 * or move the transport to align with the track.
 */
void Transport::userStart()
{
    start();
}

/**
 * The user has pressed a "Stop" button.
 *
 * Like Start, this yanks control away from the connected track.
 */
void Transport::userStop()
{
    stop();
}

/**
 * The user has requested a time signature change.
 * If the transport is running and has a unit length, this will
 * not change the length of the unit, but will influence the locations
 * of beat and bar pulses.
 *
 * NOTE: If the number is odd, this can result in roundoff errors that cause
 * the final beat to be a different size than the preceeding beats.
 * And similarly if the transport has multiple bars, the final bar may be
 * of a different size than the previous.
 *
 * If the transport is stopped and disconnected, we can reconfigure
 * the unit size to match.
 */
void Transport::userSetBeatsPerBar(int bpb)
{
    if (bpb > 0 && bpb != state.beatsPerBar) {

        state.beatsPerBar = bpb;
        if (connection == 0) {
            // todo: here we're allowed to recalculate the unit width
        }
        else {
            // todo: if the transport is connected and advancing, then
            // changing bpb can change the effective beat/bar numbers
            // that are being displayed
        }
    }
}

/**
 * Set a tempo specified by the user.
 *
 * There are two ways to do this, with a specific tempo number
 * or with a duration.
 *
 * Using a tempo number would be done when the UI offers a place
 * where a tempo can be entered manually or selected from a menu.
 *
 * Using a duration would be done when the UI provides a "tap tempo"
 * interface where the user pushes a button several times.
 *
 * If the transport is currenty connected, this will restructure
 * the transport and break the connection.
 */
void Transport::userSetTempo(float tempo)
{
    deriveUnitLength(tempo);
}

/**
 * The tempo is being set using a tap tempo duration
 * in milliseconds.
 */
void Transport::userSetTempoDuration(int millis)
{
    float samplesPerMillisecond = (float)sampleRate / 1000.0f;
    int frames = (int)((float)millis * samplesPerMillisecond);

    (void)deriveTempo(frames);
}

//////////////////////////////////////////////////////////////////////
//
// Internal Restructuring
//
//////////////////////////////////////////////////////////////////////

/**
 * Disconnect the transport from a track.
 * This has no effect other than clearing the connection number.
 * If the UI is testing isConnected, this may enable the use of
 * manual transport controls.
 */
void Transport::disconnect()
{
    connection = 0;
}

/**
 * Connect the transport to a track.
 *
 * This results in a restructuring of the transport to give it a tempo
 * and beat/bar widths that fit with the track contents.
 *
 * Here is where the magic happens.
 *
 * Try to pick the smallest basis that can be an even division of the track length.
 * If the length is short this can be treated like tap tempo.
 * If it is long then we have to guess how many "bars" should be in the track.
 *
 * This needs to be smarter, winging it ATM to get it working.
 *
 * There are many ways these calcultions could be performed, some more elegant
 * than others.  I'm not worried about elegance here, but something that is
 * obvious by reading the algorithm.
 *
 */
void Transport::connect(TrackProperties& props)
{
    connection = props.number;

    int unitFrames = props.frames;
    int resultBars = 1;

    // let's get this out of the way early
    // if the number of frames in the loop is not even, then all sorts of
    // assumptions get messy, this should have been prevented by now
    // if the number of cycles and bpb is also odd, this might result in an acceptable
    // unit, but it is sure to cause problems down the road
    if ((unitFrames % 2) != 0) {
        Trace(1, "Transport::connect Uneven loop frames %d, this will probably suck",
              unitFrames);
    }

    // try to divide by cycles it if is clean
    if (props.cycles > 1) {
        int cycleFrames = unitFrames / props.cycles;
        int expectedFrames = cycleFrames * props.cycles;
        if (expectedFrames == unitFrames) {
            // the loop divides cleanly by cycle, the cycle
            // can be the base length
            unitFrames = cycleFrames;
            resultBars = props.cycles;
        }
        else {
            // some number was odd in the loop's final length calculation
            // the last cycle will not be the same size as the others
            // the truncated cycle length can't be used as a reliable basis
            // this isn't supposed to happen if notifyTrackRecordEnding did it's job
            // but it could happen when loading random loops, or the user has taken
            // manual control over the cycle count
            // need more intelligence here
            Trace(1, "Transport: Warning: Inconsistent cycle lengths calculating base unit");
        }
    }
    
    // try to apply the user selected beatsPerBar
    if (state.beatsPerBar > 1) {
        int beatFrames = unitFrames / state.beatsPerBar;
        int expectedFrames = beatFrames * state.beatsPerBar;
        if (expectedFrames == unitFrames) {
            // it divides cleanly on beats
            unitFrames = beatFrames;
        }
        else {
            // not unexpected if they're Brubeking with bpb=5
            // this is where we should have tried to round off the ending
            // of the initial recording so it would divide clean
            // this can't be the unit without another layer of calculations
            // to deal with shortfalls and remainders
            Trace(2, "Warning: Requested Beats Per Bar %d does not like math",
                  state.beatsPerBar);
        }
    }

    // start with the usual double/halve approach to get the tempo in range
    // it could be a lot smarter here about dividing long loops into "bars"
    // rather than just assuming a backing pattern is 1,2,4,8,16 bars
    // for example if they're syncing to a 12-bar pattern and the recorded an
    // entire 12 bar loop, then we could know that
    // can't guess though without input, would need a barsPerLoop option as
    // well as beatsPerBar and barsPerLoop takes the place of the cycle count
    // if the loop wasn't already divided into cycles, that might be the easiest way

    float tempo = lengthToTempo(unitFrames);

    if (tempo > maxTempo) {
        // the loop is very short, not uncommon if it was recorded like "tap tempo"
        // and intended to be a beat length rather than a bar length
        // we could abandon BeatsParBar at this point and just find some beat subdivision
        // that results in a usable tempo but try to do what they asked for
        while (tempo > maxTempo) {
            unitFrames *= 2;
            tempo = lengthToTempo(unitFrames);
        }
    }
    else if (tempo < minTempo) {
        // the loop is very long
        // if the unit frames is uneven, then this will suck
        // should have been a forced adjustment in notifyTrackRecordEnding
        if ((unitFrames % 2) > 0) {
            Trace(1, "Transport: Rounding odd unit length %d", unitFrames);
            unitFrames--;
            // adjust = unitFrames - tapFrames;
        }
            
        while (tempo < minTempo) {
            unitFrames /= 2;
            tempo = lengthToTempo(unitFrames);
        }
    }

    state.unitFrames = unitFrames;
    state.unitsPerBeat = 1;

    // at this point a unit is a "beat" and can calculate how many bars are in the
    // resulting loop

    int barFrames = unitFrames * state.beatsPerBar;
    int bars = props.frames / barFrames;
    int expectedFrames = bars * barFrames;
    if (expectedFrames != props.frames) {
        // roundoff error, could have used ceil() here
        bars++;
    }
    state.barsPerLoop = bars;

    // now we have location
    // Connection usually happens when the loop is at the beginning, but it
    // can also happen randomly
    // lots of options here
    //     - start dealligned, and make the user manuall reallign
    //     - attempt realign and deal with MIDI song position pointer
    //     - attempt realign and leave MIDI Start till the next loop point
    //
    // If we're restructuring within the same track, then it should
    // just change the tempo without moving the MIDI position
    // 
    // If we're switching loops you might want this to restart the external device
    // much to do here
    // let SyncMaster decide

    setTempoInternal(tempo);
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

void Transport::start()
{
    state.started = true;

    // going to need a lot more state here
    if (midiEnabled) {
        midiRealizer->start();
        drifter.resync();
    }
}

void Transport::startClocks()
{
    if (midiEnabled)
      midiRealizer->startClocks();
}

void Transport::stop()
{
    pause();
    resetLocation();
}

void Transport::stopSelective(bool sendStop, bool stopClocks)
{
    Trace(1, "Transport::stopSelective");
    (void)sendStop;
    (void)stopClocks;
}

void Transport::pause()
{
    if (midiEnabled) {
        if (sendClocksWhenStopped)
          midiRealizer->stopSelective(true, false);
        else
          midiRealizer->stop();
    }
    
    state.started = false;
}

void Transport::resume()
{
    // todo: a lot more with song clocks
    start();
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

                        pulse.unit = SyncUnitLoop;
                    }
                    else {
                        pulse.unit = SyncUnitBar;
                    }
                }
                else {
                    pulse.unit = SyncUnitBeat;
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

    // for verification, purposelfy make the tempo we send to the
    // clock generator wrong
    if (testCorrection)
      midiRealizer->setTempo(tempo - 0.1f);
    else
      midiRealizer->setTempo(tempo);
    
    if (midiEnabled && sendClocksWhenStopped)
      midiRealizer->startClocks();
    
    // update the drift monitor
    drifter.setPulseFrames(state.unitFrames);
    // doesn't really matter how large this is
    drifter.setLoopFrames(state.unitFrames * state.beatsPerBar);
    // drifter will resync on the next pulse
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
 *
 * Test Evidence 1: Loki
 *
 * Drift on each loop bounces regularly between 160 and 0 with rare excursions
 * to -160 and 320.  But it always trends back to zero.
 *
 * Sample rate was 48000, tempo was 90, block size was 256.
 *
 * This feels like normal audio block jitter.
 */
void Transport::checkDrift(int blockFrames)
{
    // if we keep clocks going, could do this even when not started
    if (state.started) {
        Pulsator* pulsator = syncMaster->getPulsator();

        // Pulseator will only return Beat and Bar pulses
        Pulse* p = pulsator->getOutBlockPulse();
        int pulseOffset = (p != nullptr) ? p->blockFrame : -1;
        float adjust = drifter.advance(blockFrames, pulseOffset);
        if (adjust != 0.0f) {
            if (testCorrection) {
                float current = midiRealizer->getTempo();
                (void)current;
                // still needs work
                midiRealizer->setTempo(current + adjust);
            }
            else
              drifter.resync();
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

