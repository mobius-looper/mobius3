
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../midi/MidiByte.h"

// so we can have a pseudo-loop for tracking progress
// migth want to put that in a wrapper to keep this focused?
#include "../model/Session.h"
#include "../model/SessionHelper.h"
#include "../model/SyncState.h"
#include "../model/PriorityState.h"

#include "SyncMaster.h"
#include "MidiAnalyzer.h"

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

int MidiAnalyzer::getSmoothTempo()
{
    return smoothTempo;
}

int MidiAnalyzer::getSongPosition()
{
    return eventMonitor.songPosition;
}

/**
 * Expected to be called periodically to check whather clocks are still
 * being received.
 */
void MidiAnalyzer::checkClocks()
{
    tempoMonitor.checkStop();
    if (!tempoMonitor.isReceiving())
      resyncUnitLength = true;
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

    // important TempoMonitor first since EventMonitor may need to
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

    deriveTempo();
    
    // detect start and stop
    if (playing != eventMonitor.started) {
        if (eventMonitor.started) {
            // since we're always behind, leave the beat at block frame zero
            result.started = true;
            playing = true;
            resyncUnitLength = true;
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
        }
        else {
            Trace(2, "MidiAnalyzer: Stop");
            result.stopped = true;
            playing = false;
            resyncUnitLength = true;
        }
    }
    else if (playing) {
        if (lastMonitorBeat != eventMonitor.elapsedBeats) {
            // a native beat came in
            // if this isn't 1 away, it means we missed a beat
            // can't happen in practice unless the tempo is unusably fast
            // or you're suspending in the debugger
            if (lastMonitorBeat + 1 != eventMonitor.elapsedBeats)
              Trace(1, "MidiAnalyzer: Elapsed beat mismatch");

            if (unitLength == 0 || resyncUnitLength) {
                // virtual loop isn't running yet
                unitLocked = lockUnitLength(blockFrames, true);
            }
            else {
                // virtual loop is running, but we may be able to relock if
                // nothing is depending on it
                unitLocked = relockUnitLength(blockFrames, false);
            }
            
            lastMonitorBeat = eventMonitor.elapsedBeats;
        }
    }

    // if we didn't relock the unit length go through normal advance
    // otherwise lockUnitLength did it
    if (!unitLocked)
      advance(blockFrames);
}

/**
 * Derive the tempo used for display purposes.
 */
void MidiAnalyzer::deriveTempo()
{
    float newTempo = tempoMonitor.getAverageTempo();
    if (tempo == 0.0f && newTempo > 0.0f) {
        char buf[64];
        sprintf(buf, "MidiAnalyzer: Derived tempo %f", newTempo);
        Trace(2, buf);
    }
    tempo = newTempo;
}

/**
 * Here on each full beat from the MIDI input.
 *
 * If we decide to reorient the unit length, the play head is reset
 * and this must return true to prevent falling into the normal advance()
 * process.  The paths are slighly different if this is the initial lock after the
 * first beat, or a relock on later beats.
 * 
 * Once unit length has been calculated and there there aren't any active followers,
 * we're free to make adjustments to better smooth out the tempo for devices
 * that don't continuously send clocks.  If you don't actually start recording a track
 * until the device has played for a few bars, then we don't need to lock it until recording
 * starts.
 *
 * Alternately: We could just continually adjust the unit length on every beat until the
 * time at which a track wants to sync record.
 *
 * The virtual track will be playing at this point so changing the unit length may
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
 * to trace an error.
 */
bool MidiAnalyzer::lockUnitLength(int blockFrames, bool firstTime)
{
    (void)blockFrames;
    double clockLength = tempoMonitor.getAverageClockLength();
    int newUnitLength = tempoMonitor.getAverageUnitLength();
    // convert length to tempo to make it easier to sanity check
    int newTempo = (int)(tempoMonitor.getAverageTempo());
    bool relock = false;

    if (newUnitLength == 0) {
        // common after an emergency resync during debugging but not
        // on the first lock
        if (firstTime)
          Trace(1, "MidiAnalyzer: Unable to do first unit lock");
    }
    else if (newTempo < MidiMinTempo || newTempo > MidiMaxTempo) {
        // something went haywire in TempoMonitor, if we're not filling this is unusual
        Trace(1, "MidiAnalyzer: Ignoring unusual unit length %d", newUnitLength);
    }
    else if (tempoMonitor.isFilling() && !firstTime) {
        // common after emergency resync after debugging, continue with the
        // old length until the buffer fills
    }
    else {
        // decide whether relock is allowed

        if (unitLength == 0 || firstTime) {
            // first time here after a start point, can always relock
            relock = true;
        }
        else if (newUnitLength != unitLength) {

            // only allow a relock if there are no followers
            int followers = syncMaster->getActiveFollowers(SyncSourceMidi, unitLength);
            if (followers == 0) {
                // prevent excessive relock when the clock wobbles around
                // play around with this, 4 may be enough, but user initiated tempo changes are rare
                // and the initial guess is usually pretty close
                int relockThreshold = 8;
                int change = abs(unitLength - newUnitLength);
                if (change > relockThreshold)
                  relock = true;
            }
        }

        if (relock) {
            // on the initial lock, we're expected to go ahead and define
            // a unit even if the smoothing window isn't full, it will be less accurate
            if (tempoMonitor.isFilling() && firstTime)
              Trace(2, "MidiAnalyzer: Deriving unit during fill period, potentially unstable");

            char buf[256];
            if (unitLength == 0)
              sprintf(buf, "MidiAnalyzer: Locked unit length %d clock length %f running average %f tempo %f",
                      newUnitLength, clockLength, tempoMonitor.getAverageClock(),
                      newTempo);
            else
              sprintf(buf, "MidiAnalyzer: Adjusted unit length from %d to %d tempo %f",
                      unitLength, newUnitLength, newTempo);
            Trace(2, buf);

            unitLength = newUnitLength;

            // orient the play heaad and beging normal advancing
            // beat jumps to 1
            unitPlayHead = 0;
            if (firstTime)
              elapsedBeats = 1;
            else {
                // we're just adjusting for tempo wobble, keep the beat
                // counter going
                elapsedBeats = eventMonitor.elapsedBeats;
            }

            // since advance won't cross a beat boundary, have to do it here
            result.beatDetected = true;

            if (firstTime) {
                // stream time advance may not be exactly on a unit, but sonce we are
                // reorienting the unit length and consider this to be exactly on beta 1
                // adjust the stream time to match for drift checking
                streamTime = unitLength;
            }
            else {
                // stream time likewise goes to where it would be if we had had
                // this unit length all along
                streamTime = elapsedBeats * unitLength;
            }
        }
        else {
            char buf[256];
            sprintf(buf, "MidiAnalyzer: Supressing unit adjust from %d to %d tempo %f",
                    unitLength, newUnitLength, newTempo);
            Trace(2, buf);
        }
    }

    resyncUnitLength = false;

    return relock;
}

/**
 * Here on each beat after the first.
 *
 */
bool MidiAnalyzer::relockUnitLength(int blockFrames)
{
    (void)blockFrames;
    int newUnitLength = tempoMonitor.getAverageUnitLength();
    int newTempo = (int)(tempoMonitor.getAverageTempo());
    bool relock = false;

    if (tempoMonitor.isFilling()) {
        // normally happens only after resetting after an extreme delta,
        // commonly when suspending in the debugger, ignore until the
        // smoothing window fills again
        Trace(2, "MidiAnalyzer: Ignoring relock during smoothing");
    }
    else if (newUnitLength == 0) {
        // commonly zero immediately after resync, wait
    }
    else if (newTempo < MidiMinTempo || newTempo > MidiMaxTempo) {
        // something went haywire in TempoMonitor, if we're not filling this is unusual
        Trace(1, "MidiAnalyzer: Ignoring unusual unit length %d", newUnitLength);
    }
    else if (newUnitLength != unitLength) {
        // play around with this, 4 may be enough, but really user tempo changes are rare
        // and the initial guess is usually pretty close
        int relockThreshold = 8;
        int change = abs(unitLength - newUnitLength);
        if (change > relockThreshold) {

            // todo: count active followers
            int followers = syncMaster->getActiveFollowers(SyncSourceMidi, unitLength);
            if (followers == 0) {

                char buf[256];
                sprintf(buf, "MidiAnalyzer: Adjusting unit length from %d to %d tempo %f",
                        unitLength, newUnitLength, tempoMonitor.getAverageTempo());
                Trace(2, buf);

                unitLength = newUnitLength;

                // orient the play heaad and beging normal advancing
                // beat jumps to 1
                unitPlayHead = 0;
                elapsedBeats = eventMonitor.elapsedBeats;

                // technically we're on a beat pulse now, though nothing
                // should be following it
                result.beatDetected = true;

                // stream time likewise goes to where it would be if we had had
                // this unit length all along
                streamTime = elapsedBeats * unitLength;
            
                relock = true;
            
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
        if (unitLength == 0 || resyncUnitLength) {
            // still "recording" the initial unit
        }
        else if (unitPlayHead >= unitLength) {

            // a unit has transpired
            int blockOffset = unitPlayHead - unitLength;
            if (blockOffset > frames || blockOffset < 0)
              Trace(1, "MidiAnalyzer: You suck at math");

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
            // but once it becomes higher it is a tempo change
            int elapsedMidiClocks = tempoMonitor.getElapsedClocks();
            int expectedClocks = elapsedBeats * 24;

            if (elapsedMidiClocks != expectedClocks) {
                // drift is negative when the audio stream is behind
                Trace(2, "MidiAnalyzer: Clock drift %d", expectedClocks - elapsedMidiClocks);
            }

            // comparing stream times gives more insignt into the distance between clocks
            // it has more jitter but could be used to detect drift before it gets to the
            // point of being a full clock
            // update: This seems to be wrong and not very useful, after deliberate tempo
            // change from 120 to 121, the clock drift starts increasing as expected: -2, -3, etc.
            // but the stream drift stays about the same...math error somewhere
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
