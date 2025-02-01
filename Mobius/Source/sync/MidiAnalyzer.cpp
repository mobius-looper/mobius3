
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

    // so we can pull things from the Session
    sessionHelper.setSymbols(sm->getSymbols());
}

void MidiAnalyzer::setSampleRate(int rate)
{
    sampleRate = rate;
    tempoMonitor.setSampleRate(rate);
}

void MidiAnalyzer::shutdown()
{
    midiManager->removeRealtimeListener(this);
}

void MidiAnalyzer::loadSession(Session* s)
{
    sessionHelper.setSession(s);
    beatsPerBar = sessionHelper.getInt(ParamMidiBeatsPerBar);
    barsPerLoop = sessionHelper.getInt(ParamMidiBarsPerLoop);
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

void MidiAnalyzer::refreshState(SyncState* state)
{
    state->midiReceiving = isReceiving();
    state->midiStarted = isRunning();
    state->midiTempo = getTempo();
    state->midiNativeBeat = eventMonitor.beat;
    state->midiSongPosition = eventMonitor.songPosition;
    
    // !! need a virtual loop to play like Transport
    state->midiBeat = eventMonitor.beat;
    state->midiBar = 1; 
    state->midiLoop = 1;
    
    state->midiBeatsPerBar = beatsPerBar;
    state->midiBarsPerLoop = barsPerLoop;
    state->midiUnitLength = unitLength;
    state->midiPlayHead = unitPlayHead;
}

/**
 * Capture the priority state from the transport.
 */
void MidiAnalyzer::refreshPriorityState(PriorityState* state)
{
    state->midiBeat = eventMonitor.beat;
    state->midiBar = 1;
    state->midiLoop = 1;
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

float MidiAnalyzer::getTempo()
{
    return 120.0f;
}

/**
 * !! Not doing units and drift yet.
 */
int MidiAnalyzer::getUnitLength()
{
    return 22050;
}

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

    // important TempoMonitor first since EventMonitor may need to
    // reset it's stream time if a start point is detected
    tempoMonitor.consume(msg);
    
    bool startPoint = eventMonitor.consume(msg);
    
    if (startPoint)
      tempoMonitor.resetStreamTime();
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
            beat = 0;
            bar = 0;
            loop = 0;
            streamTime = 0;

            if (!eventMonitor.continued) {
                Trace(2, "MidiAnalyzer: Start");
            }
            else {
                Trace(2, "MidiAnalyzer: Continue %d", eventMonitor.songPosition);
                // orient the virtual loop for the song position
                // number of clocks in one bar
                int barClocks = beatsPerBar * 24;
                // the number of clocks in one virtual loop
                int loopClocks = barsPerLoop * barClocks;
                // the clock of the native song position we're starting from
                int songPositionClocks = eventMonitor.songPosition * 6;
                // the number of loops that elapsed to get there
                loop = songPositionClocks / loopClocks;
                // remaining clocks after the last virtual loop
                int remainingLoopClocks = songPositionClocks % loopClocks;
                // bars in the remainder
                bar = remainingLoopClocks / barClocks;
                // beats in the last bar
                beat = remainingLoopClocks % barClocks;
            }
        }
        else {
            Trace(2, "MidiAnalyzer: Stop");
            result.stopped = true;
            playing = false;
        }
    }
    else if (playing) {
        if (elapsedBeats != eventMonitor.elapsedBeats) {
            // if this isn't 1 away, it means we missed a beat
            // while there can be multiple clocks per block, there should
            // never be multiple beats per block unless the tempo is unusably fast
            if (elapsedBeats + 1 != eventMonitor.elapsedBeats)
              Trace(1, "MidiAnalyzer: Elapsed beat mismatch");

            if (unitLength == 0 || resyncUnitLength) {
                // virtual loop isn't running yet
                lockUnitLength(blockFrames);
            }
            else {
                // once the unit length is set, beats are determined by the
                // virtual loop
            }
            elapsedBeats = eventMonitor.elapsedBeats;
        }
    }
    
    advance(blockFrames);
}

void MidiAnalyzer::deriveTempo()
{
    float newTempo = tempoMonitor.getAverageTempo();
    if (tempo == 0.0f) {
        char buf[64];
        sprintf(buf, "MidiAnalyzer: Derived tempo %f", newTempo);
        Trace(2, buf);
    }
    tempo = newTempo;
}

/**
 * Here on the first beat after restarting.
 * If there is a unit length, it means we stopped and restarted or
 * continued after running for awhile and the resyncUnitLength flag was set.
 *
 * The play head has been advancing without generating beat pulses.
 * Once the unit length is set, control will fall into advance, whih
 * will then normally detect a beat in this block.
 */
void MidiAnalyzer::lockUnitLength(int blockFrames)
{
    (void)blockFrames;
    double clockLength = tempoMonitor.getAverageClockLength();
    int newUnitLength = tempoMonitor.getAverageUnitLength();

    if (unitLength == 0) {
        char buf[128];
        sprintf(buf, "MidiAnalyzer: Locked unit length %d clock length %f running average %f",
                newUnitLength, clockLength, tempoMonitor.getAverageClock());
        Trace(2, buf);
    }
    else if (unitLength == newUnitLength)
      Trace(2, "MidiAnalyzer: Locked unit length remains %d", unitLength);
    else 
      Trace(2, "MidiAnalyzer: Locked unit length changes from %d to %d",
            unitLength, newUnitLength);

    unitLength = newUnitLength;
    resyncUnitLength = false;

    unitLength = newUnitLength;
    unitPlayHead = 0;
    beat = 1;

    // stream time advance may not be exactly on a unit, but sonce we are
    // reorienting the unit length and consider this to be exactly on beta 1
    // adjust the stream time to match for drift checking
    streamTime = unitLength;
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
              Trace(1, "Transport: You suck at math");

            // effectively a frame wrap too
            unitPlayHead = blockOffset;

            beat++;
            
            result.beatDetected = true;
            result.blockOffset = blockOffset;

            if (beat >= beatsPerBar) {

                beat = 0;
                bar++;
                result.barDetected = true;

                if (bar >= barsPerLoop) {

                    bar = 0;
                    loop++;
                    result.loopDetected = true;
                }
            }
        }
    }
    
    streamTime += frames;
    tempoMonitor.setAudioStreamTime(streamTime);

    if (result.loopDetected) {
        Trace(2, "TempoMonitor: Drift %d", tempoMonitor.getDrift());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
