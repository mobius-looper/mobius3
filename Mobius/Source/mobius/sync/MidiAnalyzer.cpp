/**
 * Coordinates the MidiEventMonitor which detects start, stop and beats,
 * and MidiTempoMonitor which is doing clock tempo smoothing.
 *
 * Potential new parameters:
 *
 *     midiSmoothingWindow
 *        the size of the MidiTempoMonitor smoothing buffer
 *        defaults to 96 (4 beats)
 *
 *     midiWobbleThreshold
 *        the number of frames the unit length must change before we consider it
 *        defaults to 8
 *
 *     midiBpmTreshold
 *        the change in BPM that forces a unit length adjustment
 *        defaults to 1.0
 *
 *     midiDriftCheckpointBeats
 *        the number of beats that elapse between drift checkpoints
 *        defaults to 4
 *
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../midi/MidiByte.h"

// so we can have a pseudo-loop for tracking progress
// migth want to put that in a wrapper to keep this focused?
#include "../../model/Session.h"
#include "../../model/SessionHelper.h"
#include "../../model/SyncState.h"
#include "../../model/PriorityState.h"

#include "SyncMaster.h"
#include "MidiAnalyzer.h"

// todo
void MidiAnalyzer::lock()
{
}

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

/**
 * Sanity checks on tempo/unit length.
 * See wild ranges occasionally after emergency resync, should be preventing
 * those.
 */
static const int MidiMinTempo = 30;
static const int MidiMaxTempo = 300;

MidiAnalyzer::MidiAnalyzer()
{
}

MidiAnalyzer::~MidiAnalyzer()
{
}

void MidiAnalyzer::initialize(SyncMaster* sm, MidiManager* mm)
{
    syncMaster = sm;
    midiManager = mm;
    mm->addRealtimeListener(this);
}

void MidiAnalyzer::setSampleRate(int rate)
{
    sampleRate = rate;
    tempoMonitor.setSampleRate(rate);
}

void MidiAnalyzer::shutdown()
{
    shuttingDown = true;
    midiManager->removeRealtimeListener(this);
}

//////////////////////////////////////////////////////////////////////
//
// Stoppage
//
//////////////////////////////////////////////////////////////////////

/**
 * Expected to be called periodically to check whather clocks are still
 * being received.
 *
 * When this happens there are two options for the tempo display:
 * 
 * 1) Reset it so that doesn't display anything and rebuilds the tempo
 *    from scratch when the clocks restart
 *
 * 2) Leave the last tempo in place, under the assumption that the user will
 *    most likely continue or restart using the same tempo.
 *
 * What the display says about tempo isn't that important, but it is more
 * interesting to preserve the previous unit length.  If we had just spent
 * minutes smoothing out a unit length and some tracks started following that,
 * that shouldn't be immediately abandoned when clocks start up again unless
 * the tempo deviation is severe.  Due to constant jitter, starting over with
 * a new unit length could be a few samples off the previous and would result in
 * an unnecessary adjustment.
 *
 * Once the unit length is set it STAYS THERE until we're in a position to
 * reliably calculate a new one.
 */
void MidiAnalyzer::checkClocks()
{
    tempoMonitor.checkStop();
}

/**
 * When this happens all tracks will be quiet and empty and we can reset
 * any drift monitoring that may have been going on.
 * Not normally necessary, but after debugging the clocks can get way out
 * of sync and it will perpetually whine about it.
 */
void MidiAnalyzer::globalReset()
{
    elapsedBeats = tempoMonitor.getElapsedClocks() / 24;
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

/**
 * This fills in everything except the normalized
 * beat/bar/loop counts which SyncMaster/BarTender will add
 */
void MidiAnalyzer::refreshState(SyncState* state)
{
    state->midiReceiving = isReceiving();
    state->midiStarted = isRunning();
    state->midiTempo = getTempo();
    state->midiNativeBeat = eventMonitor.beat;
    state->midiSongPosition = eventMonitor.songPosition;
    
    state->midiUnitLength = unitLength;
    state->midiPlayHead = unitPlayHead;
}

//////////////////////////////////////////////////////////////////////
//
// SyncAnalyzer Interface
//
//////////////////////////////////////////////////////////////////////

SyncAnalyzerResult* MidiAnalyzer::getResult()
{
    return &result;
}

/**
 * True if we have received a MIDI start or continue message.
 */
bool MidiAnalyzer::isRunning()
{
    return eventMonitor.started;
}

int MidiAnalyzer::getNativeBeat()
{
    return eventMonitor.beat;
}

int MidiAnalyzer::getElapsedBeats()
{
    return elapsedBeats;
}

/**
 * Once the unit length is locked, display the locked tempo so the UI doesn't flicker.
 * Might want to also have a way to display the fluctuating raw tempo.
 */
float MidiAnalyzer::getTempo()
{
    if (unitLength > 0)
      return tempoMonitor.unitLengthToTempo(unitLength);
    else
      return tempoMonitor.getAverageTempo();
}

int MidiAnalyzer::getUnitLength()
{
    return unitLength;
}

/**
 * MIDI does drift a different way, and needs a different interface.
 */
int MidiAnalyzer::getDrift()
{
    return 0;
}

//////////////////////////////////////////////////////////////////////
//
// Extended Public Interface
//
//////////////////////////////////////////////////////////////////////

bool MidiAnalyzer::isReceiving()
{
    return tempoMonitor.isReceiving();
}

int MidiAnalyzer::getSongPosition()
{
    return eventMonitor.songPosition;
}

//////////////////////////////////////////////////////////////////////
//
// MidiManager::RealtimeListener
//
//////////////////////////////////////////////////////////////////////

/**
 * Given a MIDI Realtime message received from a MIDI device, add the
 * interesting ones to the input queue.
 *
 * We'll get SystemCommon messages as well as Realtime messages which
 * we need for SongPosition.  Everything else ignore.
 */
void MidiAnalyzer::midiRealtime(const juce::MidiMessage& msg, juce::String& source)
{
    (void)source;

    if (shuttingDown) return;

    // do TempoMonitor first since EventMonitor may need to
    // reset it's stream time if a start point is detected
    tempoMonitor.consume(msg);
    
    bool startPoint = eventMonitor.consume(msg);
    if (startPoint) {
        tempoMonitor.orient();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Analysis
//
//////////////////////////////////////////////////////////////////////

/**
 * Consume any queued events at the beginning of an audio block
 * and prepare the SyncAnalyzerResult
 */
void MidiAnalyzer::analyze(int blockFrames)
{
    bool unitLocked = false;
    
    result.reset();

    // detect start and stop
    if (playing != eventMonitor.started) {
        if (eventMonitor.started) {
            result.started = true;
            playing = true;
            unitPlayHead = 0;
            elapsedBeats = 0;
            streamTime = 0;
            lastMonitorBeat = 0;
            driftCheckCounter = 0;

            if (!eventMonitor.continued) {
                Trace(2, "MidiAnalyzer: Start");
            }
            else {
                Trace(2, "MidiAnalyzer: Continue %d", eventMonitor.songPosition);
                // the clock of the native song position we're starting from
                int songPositionClocks = eventMonitor.songPosition * 6;
                // the native beat number this is
                elapsedBeats = songPositionClocks / 24;
            }

            // setting this prevents advance from generating beat pulses
            // until we've received the first naitve beat
            resyncingUnitLength = true;
        }
        else {
            Trace(2, "MidiAnalyzer: Stop");
            result.stopped = true;
            playing = false;
        }

        // Start is considered a beat pulse, so don't detect another
        // one until the next full beat comes in
        lastMonitorBeat = eventMonitor.elapsedBeats;
    }
    else if (lastMonitorBeat != eventMonitor.elapsedBeats) {
        // a native beat came in
        // if this isn't 1 away, it means we missed a beat
        // can't happen in practice unless the tempo is unusably fast
        // or you're suspending in the debugger
        if (lastMonitorBeat + 1 != eventMonitor.elapsedBeats)
          Trace(1, "MidiAnalyzer: Missed beats");

        unitLocked = lockUnitLength();

        // we either calculated a new unit length or decided to reuse the
        // old one after start, can start advancing normally
        resyncingUnitLength = false;

        lastMonitorBeat = eventMonitor.elapsedBeats;
    }

    // if we didn't relock the unit length, go through normal advance
    // otherwise lockUnitLength did it
    if (!unitLocked)
      advance(blockFrames);
}

/**
 * Here on each full beat from the MIDI input.
 * This is where all the head scratching lives...
 * 
 * First go through a few ignore tests for tempo/units that are out of
 * wack, which can happen during debugging with some threads suspended
 * while the MIDI thread continues.
 *
 * Next test two thresholds to determine whether relocking the unit length is
 * allowed.
 *
 * If we decide to reorient the unit length, the play head is reset
 * and this must return true to prevent falling into the normal advance()
 * process.
 * 
 * Once unit length has been calculated and there there aren't any active followers,
 * we're free to make adjustments to better smooth out the tempo for devices
 * that don't continuously send clocks.  If you don't actually finish recording a track
 * until the device has played for a few bars, then we don't need to lock it until recording
 * ends.
 *
 * If virtual track is be playing at this point, changing the unit length may
 * cause the current playHead to be outside the unit if it was made shorter.
 * It doesn't really matter where it is since nothing should be tracking it, just
 * resync it to the start of the current native beat.  This might cause the UI
 * to go backwards a bit.
 *
 * Sigh, even with a relatively high smoothing window of 128 clock samples, this still
 * results in a unit bounce almost every beat.  It's very minor:
 *
 * MidiAnalyzer: Adjusting unit length from 32000 to 31998 tempo 90.008209
 * MidiAnalyzer: Adjusting unit length from 31998 to 32000 tempo 89.999329
 * MidiAnalyzer: Adjusting unit length from 32000 to 31998 tempo 90.007492
 * MidiAnalyzer: Adjusting unit length from 31998 to 32000 tempo 90.001404
 * MidiAnalyzer: Adjusting unit length from 32000 to 31998 tempo 90.003922
 * MidiAnalyzer: Adjusting unit length from 31998 to 32000 tempo 90.002602
 *
 * To further reduce noise, don't relock unless the unit length changes more
 * than some number of samples from it's current value.  Needs to be tunable...
 * Should further suppress the ocassional outlier that jumps way out of wack
 * then comes back down.  It needs to be sustained in one direction before we relock.
 *
 * Tempo anomolies that result in a resync of the tempo monitor, or wildly out
 * of range tempos are common when you've been stopped in the debugger while
 * MIDI clocks keep coming in.  Suppress those, but they're unexpected normally
 * so trace an error.
 */
bool MidiAnalyzer::lockUnitLength()
{
    bool relock = false;
    int newUnitLength = tempoMonitor.getAverageUnitLength();
    // convert length to a clipped tempo to make it easier to sanity check
    int newClippedTempo = (int)(tempoMonitor.getAverageTempo());

    if (newUnitLength == 0) {
        // common after an emergency resync during debugging but not
        // on the first lock
        if (unitLength == 0)
          Trace(1, "MidiAnalyzer: Unable to do first unit lock");
    }
    else if (newClippedTempo < MidiMinTempo || newClippedTempo > MidiMaxTempo) {
        // something went haywire in TempoMonitor, if we're not filling this is unusual
        Trace(1, "MidiAnalyzer: Ignoring unusual unit length %d", newUnitLength);
    }
    else if (tempoMonitor.isFilling() && unitLength != 0) {
        // common after emergency resync after debugging, continue with the
        // old length until the buffer fills
        //Trace(2, "MidiAnalyzer: Waiting for tempo smoother to fill");
    }
    else if (newUnitLength != unitLength) {

        // cut down on noise by suppressing minor wobbles
        // play around with this, 4 may be enough, but user initiated tempo changes are rare
        // and the initial guess is usually pretty close
        int wobbleThreshold = 8;
        int change = abs(unitLength - newUnitLength);
        if (change <= wobbleThreshold) {
            // this is likely to be noisy, remove it after verified that it works
            //Trace(2, "MidiAnalyzer: Suppressing unit adjust within wobble range %d to %d",
            //unitLength, newUnitLength);
        }
        else {
            // derive the new tempo from the unit length
            // if the tempo changes by one full BPM it relocks, regardless of followers
            // followers if any become disconnected and drift free
            float newTempo = tempoMonitor.unitLengthToTempo(newUnitLength);
            int bpmDelta = abs((int)(tempo - newTempo));
            if (bpmDelta >= 1) {
                if (unitLength > 0) {
                    char buf[256];
                    sprintf(buf, "MidiAnalyzer: Relocking after BPM change %f to %f",
                            tempo, newTempo);
                    Trace(2, buf);
                }
                relock = true;
            }
            else {
                // relock is allowed if there are no followers
                int followers = syncMaster->getActiveFollowers(SyncSourceMidi, unitLength);
                if (followers == 0)
                  relock = true;
                else {
                    // turn this off after testing, it's not that informative once things are working
                    char buf[256];
                    sprintf(buf, "MidiAnalyzer: Supressing follower unit adjust from %d to %d tempo %f",
                            unitLength, newUnitLength, newTempo);
                    Trace(2, buf);
                }
            }

            if (relock) {
                char buf[256];
                if (unitLength > 0) {
                    sprintf(buf, "MidiAnalyzer: Adjusting unit length from %d to %d tempo %f",
                            unitLength, newUnitLength, newTempo);
                }
                else {
                    // include a little extra trace the first time we lock
                    sprintf(buf, "MidiAnalyzer: Locked unit length %d clock length %f running average %f tempo %f",
                            newUnitLength,
                            tempoMonitor.getAverageClockLength(), tempoMonitor.getAverageClock(),
                            newTempo);
                    // on the initial lock, we're expected to go ahead and define
                    // a unit even if the smoothing window isn't full, it will be less accurate
                    // but may be adjusted over time if the recording goes on long enough
                    if (tempoMonitor.isFilling())
                      Trace(2, "MidiAnalyzer: Deriving unit during fill period, potentially unstable");
                }
                Trace(2, buf);

                unitLength = newUnitLength;
                tempo = newTempo;

                // orient the play heaad and beging normal advancing
                // beat jumps to 1
                unitPlayHead = 0;

                // we're perfectly aligned with the native beat count
                // this will be 1 if we're starting, greater if continuing
                elapsedBeats = eventMonitor.elapsedBeats;
                streamTime = elapsedBeats * unitLength;

                // since advance() won't be called, indiciate the result beat
                result.beatDetected = true;
            }
        }
    }

    return relock;
}

/**
 * Advance the pseudo loop and keep track of beat bar boundaries
 *
 * This one is weirder than transport because we detect beats based on
 * events actually received, so it's more like HostAnalyzer
 */
void MidiAnalyzer::advance(int frames)
{
    if (playing) {
        unitPlayHead += frames;
        if (unitLength == 0 || resyncingUnitLength) {
            // still waiting for the native first beat after starting
            // to see whether to adjust the previoius unit length
        }
        else if (unitPlayHead >= unitLength) {
            // a unit has transpired
            
            int blockOffset = unitPlayHead - unitLength;
            if (blockOffset > frames || blockOffset < 0) {
                // this has happened after suspending in the debugger and
                // the threads start advancing in unusual ways
                // or maybe you're just bad at this
                Trace(1, "MidiAnalyzer: The universe is wrong and/or you suck at math");
                // don't let bizarre buffer offsets escape and confuse the TimeSlicer
                blockOffset = 0;
            }

            // effectively a frame wrap too
            unitPlayHead = blockOffset;

            elapsedBeats++;
            
            result.beatDetected = true;
            result.blockOffset = blockOffset;
        }
    }

    // now that we don't have bar/loop detection down here, need a good point
    // to check for drift, every 4 beats seems fine
    
    if (result.beatDetected) {

        driftCheckCounter++;
        // should be configurable!
        if (driftCheckCounter >= 4) {
            driftCheckCounter = 0;
            
            // perfection is when our elapsed best counter matches the
            // the MIDI thread's elapsed clock counter
            // it will often be 1 clock lower or higher due to normal jitter
            // but once it becomes higher it is a significant tempo drift
            // and should be corrected
            int elapsedMidiClocks = tempoMonitor.getElapsedClocks();
            int expectedClocks = elapsedBeats * 24;

            // drift is negative when the audio stream is behind
            int drift = expectedClocks - elapsedMidiClocks;

            if (abs(drift) > 1) {
                Trace(2, "MidiAnalyzer: Clock drift %d", drift);

                // todo: the magic of drift correction
            }

            // this was an experiment that didn't work
            // all TempoMonitor stream time does is bring it up to "now" time
            // which is more or less the same as audio stream time
            // it detects audio block jitter but that isn't interresting for correction
            // watching clock counts works well enough, can abandon this
#if 0        
            int midiStreamTime = tempoMonitor.getStreamTime();
            // do this before including the advance in this block
            Trace(2, "MidiAnalyzer: Stream drift %d", streamTime - midiStreamTime);
#endif        
        }
    }
    
    streamTime += frames;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

