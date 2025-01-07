
#include <JuceHeader.h>

#include "../midi/MidiByte.h"

#include "MidiQueue.h"
#include "MidiSyncEvent.h"
#include "MidiAnalyzer.h"

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

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

/**
 * Expected to be called periodically to check whather clocks are still
 * being received.
 */
void MidiAnalyzer::checkClocks()
{
    int now = juce::Time::getMillisecondCounter();
    inputQueue.checkClocks(now);
}

bool MidiAnalyzer::isReceiving()
{
    return inputQueue.receivingClocks;
}

float MidiAnalyzer::getTempo()
{
    return tempoMonitor.getTempo();
}

int MidiAnalyzer::getSmoothTempo()
{
    return tempoMonitor.getSmoothTempo();
}

int MidiAnalyzer::getBeat()
{
    return inputQueue.beat;
}

int MidiAnalyzer::getSongClock()
{
    return inputQueue.songClock;
}

/**
 * True if we have received a MIDI start or continue message.
 */
bool MidiAnalyzer::isStarted()
{
    return inputQueue.started;
}

/**
 * Package the state bits into a single thing.
 */
void MidiAnalyzer::getState(SyncSourceState& state)
{
    state.tempo = tempoMonitor.getTempo();
    state.smoothTempo = tempoMonitor.getSmoothTempo();

    // this is the raw beat from the last known START or CONTINUE
    state.beat = inputQueue.beat;
    state.songClock = inputQueue.songClock;
    state.started = inputQueue.started;
}

//////////////////////////////////////////////////////////////////////
//
// Events
//
//////////////////////////////////////////////////////////////////////

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

MidiSyncEvent* MidiAnalyzer::popEvent()
{
    return inputQueue.popEvent();
}

void MidiAnalyzer::startEventIterator()
{
    inputQueue.iterateStart();
}

MidiSyncEvent* MidiAnalyzer::nextEvent()
{
    return inputQueue.iterateNext();
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
