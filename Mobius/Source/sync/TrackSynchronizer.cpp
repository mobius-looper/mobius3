

#include <JuceHeader.h>

#include "../Supervisor.h"

#include "MidiSyncEvent.h"
#include "MidiQueue.h"

#include "TrackSynchronizer.h"

TrackSynchronizer::TrackSynchronizer(Supervisor* s)
{
    supervisor = s;
}

TrackSynchronizer::~TrackSynchronizer()
{
}


void TrackSynchronizer::initialize()
{
    // this is MidiRealizer accessed through MobiusInterface as
    // a MobiusMidiTransport
    // when you factor out a general sync library, this could probably
    // be owned by the synchronizer
    midiTransport = supervisor->getMidiRealizer();

}

//////////////////////////////////////////////////////////////////////
//
// Events and Pool
//
//////////////////////////////////////////////////////////////////////

/**
 * Return events generated on the last interrupt to the pool.
 */
void TrackSynchronizer::flushEvents()
{
    while (events != nullptr) {
        Event* next = events->next;
        events->next = eventPool;
        eventPool = events;
        events = next;
    }
}

TrackSynchronizer::Event* TrackSynchronizer::newEvent()
{
    Event* event = nullptr;
    
    // todo: need a real pool
    if (eventPool != nullptr) {
        event = eventPool;
        eventPool = eventPool->next;

        event->millisecond = 0;
        event->pulse = PulseNone;
        event->continuePulse = -1;
        event->beat = 0;
        event->frame = 0;
    }
    else {
        event = new Event();
    }
    return event;
}

void TrackSynchronizer::freeEvent(Event* event)
{
    event->next = eventPool;
    eventPool = event;
}

void TrackSynchronizer::interruptStart(MobiusAudioStream* stream)
{
    // capture some statistics
	lastInterruptMsec = interruptMsec;
	interruptMsec = midiTransport->getMilliseconds();
	interruptFrames = stream->getInterruptFrames();

    flushEvents();
    gatherMidi();
    gatherHost(stream);
}

//////////////////////////////////////////////////////////////////////
//
// MIDI In & Out
//
//////////////////////////////////////////////////////////////////////

/**
 * Assimilate queued MIDI realtime events from the MIDI transport.
 *
 * old notes:
 * We'll get UNIT_BEAT events here, to detect UNIT_BAR we have
 * to apply BeatsPerBar from the Setup
 * NOTE: in theory BPB can be track specific if we fall back to the Preset
 * that would mean we have to recalculate the pulses for every Track,
 * I really dont' think that's worth it
 */
void TrackSynchronizer::gatherMidi()
{
    MidiQueue::Iterator iterator;
    
    int bpb = getMidiInBeatsPerBar();
    midiTransport->iterateInput(iterator);
    
    MidiSyncEvent* mse = iterator.next();
    while (mse != nullptr) {
        Event* event = convertEvent(mse, bpb);
        event->source = SourceMidiIn;
        event->next = events;
        events = event;
        mse = iterator.next();
    }

    // again for internal output events
    bpb = getMidiOutBeatsPerBar();
    midiTransport->iterateOutput(iterator);

    mse = iterator.next();
    while (mse != nullptr) {
        Event* event = convertEvent(mse, bpb);
        event->source = SourceMidiOut;
        event->next = events;
        events = event;
        mse = iterator.next();
    }
}

/**
 * Convert a MidiSyncEvent from the transport into a synchronizer Event.
 * todo: this is place where we should try to offset the event into the buffer
 * to make it align more accurately when real time.
 */
TrackSynchronizer::Event* TrackSynchronizer::convertEvent(MidiSyncEvent* mse, int beatsPerBar)
{
    Event* event = newEvent();

    event->millisecond = mse->millisecond;
    if (mse->isStop) {
        event->type = TypeStop;
    }
    else if (mse->isStart) {
        event->type = TypeStart;
        event->pulse = PulseBeat;
    }
    else if (mse->isContinue) {
        event->type = TypeContinue;
        event->continuePulse = mse->songClock;
        // If we're exactly on a beat boundary, set the continue
        // pulse type so we can treat this as a beat pulse later
        if (mse->isBeat)
          event->pulse = PulseBeat;
    }
    else {
        // ordinary clock
        event->type = TypePulse;
        if (!mse->isBeat) {
            event->pulse = PulseClock;
        }
        else {
            event->pulse = PulseBeat;
            event->beat = mse->beat;
        }
    }

    // upgrade Beat pulses to Bar pulses if we're on a bar
    if (event->type == TypePulse && 
        event->pulse == PulseBeat &&
        beatsPerBar > 0) {

        if ((event->beat % beatsPerBar) == 0)
          event->pulse = PulseBar;
    }
    
	return event;
}

/**
 * Old Synchronizer did something godawful here, rethink
 */
int TrackSynchronizer::getMidiInBeatsPerBar()
{
    return 4;
}

int TrackSynchronizer::getMidiOutBeatsPerBar()
{
    return 4;
}

//////////////////////////////////////////////////////////////////////
//
// Host
//
//////////////////////////////////////////////////////////////////////

/**
 * Host events
 * Unlike MIDI events which are quantized by the MidiQueue, these
 * will have been created in the *same* interrupt and will have frame
 * values that are offsets into the current interrupt.
 *
 * old notes:
 * These must be maintained in order and interleaved with the loop events.
 *
 * newnotes:
 * Wtf does that mean?  How many host events will get get in one interrupt.
 * Just Start followed by beat, right?  If so could merge these like we
 * do for MIDI?
 * DO we really even need Start events?  Everything shouldl be waiting for the
 * Beat after the Start.
 */
void TrackSynchronizer::gatherHost(MobiusAudioStream* stream)
{
    // refresh host sync state for the status display in the UI thread
	AudioTime* hostTime = stream->getAudioTime();
    if (hostTime == NULL) {
        // can this happen, reset everyting or leave it where it was?
        /*
        mHostTempo = 0.0f;
        mHostBeat = 0;
        mHostBeatsPerBar = 0;
        mHostTransport = false;
        mHostTransportPending = false;
        */
        Trace(1, "TrackSynchronizer: Unexpected null AudioTime");
    }
    else {
		hostTempo = (float)hostTime->tempo;
		hostBeat = hostTime->beat;
        hostBeatsPerBar = hostTime->beatsPerBar;
        
        // stop is always non-pulsed
        if (hostTransport && !hostTime->playing) {
            Event* event = newEvent();
            event->source = SourceHost;
            event->type = TypeStop;
            // todo: order insertion here?
            event->next = events;
            events = event;
            hostTransport = false;
        }
        else if (hostTime->playing && !hostTransport) {
            hostTransportPending = true;
        }
        
        // should this be an else with handling transport stop above ?
        // what about CONTINUE, will we always be on a boundary?
        if (hostTime->beatBoundary || hostTime->barBoundary) {

            Event* event = newEvent();
            event->source = SourceHost;
            event->frame = hostTime->boundaryOffset;

            // If the transport state changed or if we did not
            // advance the beat by one, assume we can do a START/CONTINUE.
            // This isn't critical but it's nice with host sync so we can 
            // reset the avererage pulse width calculator which may be
            // way off if we're jumping the host transport.

            // NEW: This logic was messing up FL Studio and probably other pattern-based
            // hosts that don't increase beat numbers monotonically, it jumps back to zero
            // on every cycle.  In general, Mobius isn't ready to be smart about
            // following the host transport.  The old sync code is an absoluate mess
            // that needs to be taken out back and burned.  Don't try to be smart about this.
            // get the fundamental beat/bar sync working and worry about trying to follow
            // pause/continue later since it is far more complicated than just this
            // if (mHostTransportPending || ((lastBeat + 1) != mHostBeat)) {
            
            if (hostTransportPending) {

                // NEW: not sure if we should even try to mess with CONTINUE
                // whenever the transport starts, just send START?
                bool alwaysStarts = false;
                
                if (alwaysStarts ||  hostBeat == 0) {
                    event->type = TypeStart;
                    event->pulse = PulseBar;
                }
                else {
                    event->type = TypeContinue;
                    // continue pulse is the raw pulse not rounded for bars
                    event->continuePulse = hostBeat;
                    if (hostTime->barBoundary)
                      event->pulse = PulseBar;
                    else
                      event->pulse = PulseBeat;
                }
                hostTransport = true;
                hostTransportPending = false;
            }
            else {
                event->type = TypePulse;
                if (hostTime->barBoundary)
                  event->pulse = PulseBar;
                else
                  event->pulse = PulseBeat;
            }

            event->next = events;
            events = event;
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

