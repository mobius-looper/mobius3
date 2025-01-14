
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../midi/MidiByte.h"

#include "MidiQueue.h"
#include "MidiSyncEvent.h"
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
}

void MidiAnalyzer::shutdown()
{
    midiManager->removeRealtimeListener(this);
}

void MidiAnalyzer::refreshState(SyncState& state)
{
    state.receiving = inputQueue.receivingClocks;
    state.tempo = tempoMonitor.getTempo();
    // this is the raw beat from the last known START or CONTINUE
    state.beat = inputQueue.beat;

    // bars, beatsPerBar, and barsPerLoop must be done at a higher level
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
    return tempoMonitor.getTempo();
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
        detectBeat(mse);
        mse = inputQueue.iterateNext();
    }
    inputQueue.flushEvents();
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
        result.stopped = true;
    }
    else if (mse->isStart) {
        // MidiRealizer deferred this until the first clock
        // after the start message, so it is a true beat
        detected = true;
        result.started = true;
    }
    else if (mse->isContinue) {
        // !! this needs a shit ton of work
        // only pay attention to this if this is also a beat pulse
        // not sure if this will work, but I don't want to fuck with continue right now
        // treat it like a Start and ignore song position
        if (mse->isBeat) {
            detected = true;
            result.started = true;

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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
