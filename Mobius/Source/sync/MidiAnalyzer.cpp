
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../midi/MidiByte.h"

// so we can have a pseudo-loop for tracking progress
// migth want to put that in a wrapper to keep this focused?
#include "../model/Session.h"
#include "../model/SessionHelper.h"
#include "../model/SyncState.h"
#include "../model/PriorityState.h"

#include "MidiQueue.h"
#include "MidiSyncEvent.h"
#include "SyncMaster.h"
#include "MidiAnalyzer.h"

//////////////////////////////////////////////////////////////////////
//
// Configuration
//
//////////////////////////////////////////////////////////////////////

MidiAnalyzer::MidiAnalyzer()
{
	inputQueue.setName("external");
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

    state->midiBeat = inputQueue.beat;
    // need a virtual loop to play like Transport
    state->midiBar = 1; 
    state->midiLoop = 1;
    
    // need to be capturing these from the session
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
    state->midiBeat = inputQueue.beat;
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
    return inputQueue.started;
}

/**
 * !! Is this really elapsed or did MidiQueue orient it for SongPosition?
 */
int MidiAnalyzer::getElapsedBeats()
{
    return inputQueue.beat;
}

float MidiAnalyzer::getTempo()
{
    //return tempoMonitor.getTempo();
    return (float)(tempoMonitor.getSmoothTempo()) / 10.0f;
}

/**
 * !! Not doing units and drift yet.
 */
int MidiAnalyzer::getUnitLength()
{
    return 0;
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
    return inputQueue.receivingClocks;
}

int MidiAnalyzer::getSmoothTempo()
{
    return tempoMonitor.getSmoothTempo();
}

int MidiAnalyzer::getSongClock()
{
    return inputQueue.songClock;
}

/**
 * Allow enabling and disabling of MidiSyncEvents in cases where
 * Mobius may not be responding and we don't want to overflow the event buffer.
 */
void MidiAnalyzer::disableEvents()
{
    inputQueue.setEnableEvents(false);
}

void MidiAnalyzer::enableEvents()
{
    inputQueue.setEnableEvents(true);
}

void MidiAnalyzer::flushEvents()
{
    inputQueue.flushEvents();
}

/**
 * Expected to be called periodically to check whather clocks are still
 * being received.
 */
void MidiAnalyzer::checkClocks()
{
    int now = juce::Time::getMillisecondCounter();
    inputQueue.checkClocks(now);
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
    
    const juce::uint8* data = msg.getRawData();
    const juce::uint8 status = *data;
    int now = juce::Time::getMillisecondCounter();
    
	switch (status) {
		case MS_QTRFRAME: {
			// not sure what this is, ignore
		}
		break;
		case MS_SONGPOSITION: {
			// only considered actionable if a MS_CONTINUE is received later
            // does not generate a MidiSyncEvent, just save it
            // I'm not sure what Juce does with this value, assume it's
            // the usual combination of message bytes
            inputQueue.setSongPosition(msg.getSongPositionPointerMidiBeat());
		}
		break;
		case MS_SONGSELECT: {
			// nothing meaningful for Mobius?
			// could use it to select loops?
		}
		break;
		case MS_CLOCK: {
            inputQueue.add(status, now);
            tempoMonitor.clock(now);
		}
		break;
		case MS_START: {
			inputQueue.add(status, now);
		}
		break;
		case MS_STOP: {
			inputQueue.add(status, now);
		}
		break;
		case MS_CONTINUE: {
			inputQueue.add(status, now);
		}
		break;
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
    (void)blockFrames;
    
    result.reset();

    inputQueue.iterateStart();
    MidiSyncEvent* mse = inputQueue.iterateNext();
    while (mse != nullptr) {
        if (inputQueue.isStarted())
          detectBeat(mse);
        mse = inputQueue.iterateNext();
    }
    inputQueue.flushEvents();

    advance(blockFrames);
}

/**
 * Convert a queued MidiSyncEvent into fields in the SyncAnalyzerResult
 * for later consumption by Pulsator.
 * 
 * todo: this is place where we should try to offset the event into the buffer
 * to make it align more accurately with real time.
 *
 * This still queues queues MidiSyncEvents for each clock although only
 * one of them should have the beat flag set within one audio block.
 */
void MidiAnalyzer::detectBeat(MidiSyncEvent* mse)
{
    bool detected = false;
    
    if (mse->isStop) {
        stopDetected();
    }
    else if (mse->isStart) {
        // MidiRealizer deferred this until the first clock
        // after the start message, so it is a true beat
        detected = true;
        startDetected();
    }
    else if (mse->isContinue) {
        // note: There is no actual "continue" in MIDI
        // there is 0xF2 "song position" followed by 0xFA Start
        
        // !! this needs a shit ton of work
        // only pay attention to this if this is also a beat pulse
        // not sure if this will work, but I don't want to fuck with continue right now
        // treat it like a Start and ignore song position
        if (mse->isBeat) {
            detected = true;
            continueDetected(mse->songClock);

            // this is how older code adjusted the Pulse
            //pulse->mcontinue = true;
            //pulse->continuePulse = mse->songClock;
        }
    }
    else {
        // ordinary clock
        // ignore if this isn't also a beat
        detected = (mse->isBeat);
    }

    if (detected && result.beatDetected) {
        // more than one beat in this block, bad
        Trace(1, "MidiAnalyzer: Multiple beats detected in block");
    }
    
    result.beatDetected = detected;
}

void MidiAnalyzer::startDetected()
{
    Trace(2, "MidiAnalyzer: Start");
    result.started = true;
    drifter.orient(unitLength);

    unitPlayHead = 0;
    elapsedBeats = 0;
    //lastBeatTime = 0;
}

void MidiAnalyzer::stopDetected()
{
    Trace(2, "MidiAnalyzer: Stop");
    result.stopped = true;
}

void MidiAnalyzer::continueDetected(int songClock)
{
    Trace(2, "MidiAnalyzer: Continue %d", songClock);
    result.started = true;
}

/**
 * Advance the pseudo loop and keep track of beat bar boundaries
 *
 * This one is weirder than transport because we detect beats based on
 * events actually received, so it's more like HostAnalyzer
 */
void MidiAnalyzer::advance(int frames)
{
    if (inputQueue.started && unitLength > 0) {

        unitPlayHead = unitPlayHead + frames;
        if (unitPlayHead >= unitLength) {

            // a unit has transpired
            int blockOffset = unitPlayHead - unitLength;
            if (blockOffset > frames || blockOffset < 0)
              Trace(1, "Transport: You suck at math");

            // effectively a frame wrap too
            unitPlayHead = blockOffset;

            elapsedBeats++;
            beat++;
            
            //result.beatDetected = true;
            //result.blockOffset = blockOffset;

            if (!result.beatDetected)
              Trace(1, "MidiAnalyzer: Beat not detected where we thought");

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

        // todo: also advance the drift monitor
        //drifter.advanceStreamTime(frames);
    }

    if (result.loopDetected)
      checkDrift();
}

void MidiAnalyzer::checkDrift()
{
    //int drift = drifter.getDrift();
    //if (drift > 256)
    //Trace(2, "Transport: Drift %d", drift);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
