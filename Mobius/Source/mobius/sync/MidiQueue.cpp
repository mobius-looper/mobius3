/**
 * Utility class used to accumulate MIDI realtime messages,
 * and convert them to a simplified model closer to what the
 * Mobius engine wants to process.  While this was designed for
 * Mobius, it is general and should be kept independent so it may
 * be reused by other things.
 *
 * The most significant thing this does is monitor clock messages
 * to determine which clocks represent Start and Continue transport
 * events and which clocks represent "beats".
 *
 * The MIDI standard defines a beat or quarter note as 24 clocks.
 * It further defines the onset of a Start or Continue as the first
 * clock AFTER a Start (0xFA) or Continue (0xFB) message is received.
 * 
 * Most MIDI applications will need a little state machine to figure out
 * where exactly start/continue/beat events are in the raw MIDI message
 * stream.  This is it.
 *
 * Note that there is no "bar" concept at this level since that is
 * not part of the standard and more of an arbitrary user preference.  
 *
 */

#include <memory.h>

#include "../../util/Trace.h"
#include "../../midi/MidiByte.h"
#include "SyncTrace.h"
#include "MidiSyncEvent.h"

#include "MidiQueue.h"

//////////////////////////////////////////////////////////////////////
//
// Constants
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the maximum number of milliseconds that can appear between
 * MS_CLOCK events before we consider that the clock stream has stopped.
 * Used in the determination of the MidiState.receivingClocks field, 
 * which is in turn exposed as the syncInReceiving script variable.
 *
 * Some BPM/clock ratios to consider:
 *
 * 60 bpm = 24 clocks/second
 * 15 bpm = 7 clocks/second
 * 7.5 bpm =  1.5 clocks/second
 * 1.875 bpm = .75 clocks/second
 *
 * If the clock rate drops below 10 bpm we should be able to consider
 * that "not receiving", for the purpose of the syncInReceiving variable.
 * 7.5 bpm is 666.66 milliseconds.
 *
 * Get thee behind me Satan!
 */
#define MAXIMUM_CLOCK_DISTANCE 666

//////////////////////////////////////////////////////////////////////
//
// MidiSyncEvent
//
//////////////////////////////////////////////////////////////////////

void MidiSyncEvent::reset()
{
    millisecond = 0;
    isStop = false;
    isStart = false;
    isContinue = false;
    isBeat = false;
    beat = 0;
    songClock = 0;
}

//////////////////////////////////////////////////////////////////////
//
// MidiQueue
//
//////////////////////////////////////////////////////////////////////

MidiQueue::MidiQueue()
{
	memset(&events, 0, sizeof(events));
}

MidiQueue::~MidiQueue()
{
}

/**
 * Set a name to disambiguate the queues in Trace messages.
 * The name must be a string constant.
 */
void MidiQueue::setName(const char* name)
{
	queueName = name;
}

/**
 * Once events are enabled the owner MUST either pop them or
 * flush them on every block advance or else the queue will overflow.
 */
void MidiQueue::setEnableEvents(bool b)
{
    enableEvents = b;
}

bool MidiQueue::isStarted()
{
    return started;
}

/**
 * Expected to be called at regular intervals with the current system time
 * so we can detect sudden clock stoppages, such as turning off or
 * disconnecting a device previouisly in use.
 *
 * Old code did this in Synchronizer at the beginning of each audio block,
 * but it can just as easily be done by the maintenance thread.
 */
void MidiQueue::checkClocks(int millisecond)
{
	if (receivingClocks) {
		int delta = millisecond - lastClockMillisecond;
		if (delta > MAXIMUM_CLOCK_DISTANCE) {
			Trace(2, "MidiQueue %s: Stopped receiving clocks\n", queueName);
			receivingClocks = false;
		}
	}
}

/**
 * Called when a MIDI SongPosition message is received.
 * These don't generate events, just save the position and include it
 * in the event the next time a Continue is received.
 */
void MidiQueue::setSongPosition(int pos)
{
    songPosition = pos;
}

/**
 * Reserve a new sync event to be filled in later.
 * 
 * The event list is "empty" when the tail and head indexes are the same.
 * The head index always points to the next available event to use for
 * new events, but it cannot be used unless it is allowed to increment without
 * running into the tail.
 */
MidiSyncEvent* MidiQueue::reserveEvent()
{
    MidiSyncEvent* e = nullptr;

	int nextHead = eventHead + 1;
	if (nextHead >= MaxSyncEvents)
	  nextHead = 0;

    if (nextHead == eventTail) {
		// overflow, should only happen if the audio interrupt is stuck
        // or a MIDI device is going haywire
		// don't emit any trace in this case, since we're likely
		// to generate a LOT of messages
		eventOverflows++;
    }
    else {
        e = &(events[eventHead]);
        e->reset();
    }

    return e;
}

/**
 * Commit a previously reserved sync event that is now ready to
 * be consumed.
 */
void MidiQueue::commitEvent()
{
	int nextHead = eventHead + 1;
	if (nextHead >= MaxSyncEvents)
	  nextHead = 0;

    if (nextHead != eventTail) {
        eventHead = nextHead;
    }
    else {
        // can't be here, would have been caught in reserveEvent
        // if things were working
        Trace(1, "MidiQueue %s: Event list flys in the face of all logic!\n",
              queueName);
    }
}

/**
 * Ladies and gentlemen, this is why you're here.
 * 
 * Assimilate one of the realtime messages: Start, Continue, Stop, or Clock.
 * This may or not allocate a MidiSyncMessage.
 *
 * Note: Old code was weird around handling of start/continue when we
 * we already in a started state.  I'm not sure what it was trying to accomplish
 * but it was probably wrong.  In theory a Start after we're running could
 * restart the loop(s) and Continue could move the playback position, but there
 * are a whole host of issues there.  Just ignore them if they come in
 * unexpected.
 */
void MidiQueue::add(int status, int millisecond)
{
	switch (status) {

		case MS_START: {
            if (started) {
                Trace(2, "MidiQueue %s: Ignoring redundant Start message\n", queueName);
            }
            else {
                // arm start event for the next clock
                waitingStatus = status;
                // this is also considered a "clock" for the purpose
                // of detecting activity in the stream
                lastClockMillisecond = millisecond;
            }
		}
		break;

		case MS_CONTINUE: {
            if (started) {
                Trace(2, "MidiQueue %s: Ignoring Continue message while started\n", queueName);
            }
            else {
                // arm continue for the next clock
                waitingStatus = status;
                lastClockMillisecond = millisecond;
            }
		}
		break;

		case MS_STOP: {
            Trace(2, "MidiQueue %s: Stop\n", queueName);
			waitingStatus = 0;
			songPosition = -1;
            songClock = 0;
            beatClock = 0;
            beat = 0;
			started = false;

            if (enableEvents) {
                MidiSyncEvent* e = reserveEvent();
                if (e != nullptr) {
                    e->isStop = true;
                    e->millisecond = millisecond;
                    commitEvent();
                }
            }
        }
		break;
		
		case MS_CLOCK: {

			// Check for resurection of the clock stream for the
			// syncInReceiving variable.  If the clocks stop, this
			// will be detected in the checkClocks method.
			int delta = millisecond - lastClockMillisecond;
			lastClockMillisecond = millisecond;
			if (!receivingClocks && delta <  MAXIMUM_CLOCK_DISTANCE) {
				Trace(2, "MidiQueue %s: Started receiving clocks\n", queueName);
				receivingClocks = true;
			}

            bool isStartClock = false;
            bool isContinueClock = false;
            bool isBeatClock = false;
            
            if (waitingStatus == MS_START) {
				Trace(2, "MidiQueue %s: Start\n", queueName);
                waitingStatus = 0;
                isStartClock = true;
                
				songPosition = -1;
				songClock = 0;
				beatClock = 0;
				beat = 0;
				started = true;
            }
            else if (waitingStatus == MS_CONTINUE) {
                waitingStatus = 0;
                isContinueClock = true;
                // new: gak, song position makes my brain hurt
                // and I don't think I ever did it right...
                // these are old comments:
				// use songPosition if it was set, otherwise keep
				// going from where are, would it be better to 
				// assume starting from zero??
				if (songPosition >= 0)
				  songClock = songPosition * 6;
				songPosition = -1;
				beatClock = songClock % 24;
				beat = songClock / 24;
				started = true;
                Trace(2, "MidiQueue %s: Continue songClock %d\n", queueName, songClock);
            }
            else {
                // a normal old clock
				waitingStatus = 0;
				songClock++;
				beatClock++;
				if (beatClock >= 24) {
                    isBeatClock = true;
					beat++;
					beatClock = 0;
				}
                if (SyncTraceEnabled)
                  Trace(2, "Sync: Queue clock beatClock %d beat %d",
                        beatClock, beat);
			}

            // formerly generated an event for every clock, but Pulsator doesn't
            // care any more and can do drift correction just fine with beats
            // only generate events on beats
            if (enableEvents &&
                (isStartClock || isContinueClock || isBeatClock)) {
                
                MidiSyncEvent* event = reserveEvent();
                if (event != nullptr) {
                
                    event->millisecond = millisecond;
                    event->isStart = isStartClock;
                    event->isContinue = isContinueClock;
                    if (isContinueClock)
                      event->songClock = songClock;
                
                    if (beatClock == 0) {
                        event->isBeat = true;
                        event->beat = beat;

                        if (traceEnabled)
                          Trace(2, "MQ: Beat");
                    }

                    if (SyncTraceEnabled)
                      Trace(2, "Sync: Generated sync event");
                    
                    commitEvent();
                }
            }
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////
//
// Event Consumption
//
//////////////////////////////////////////////////////////////////////

/**
 * Return true if we have any events to process.
 */
bool MidiQueue::hasEvents()
{
    return (eventHead != eventTail);
}

/**
 * Return the next event in the queue.
 * This is one way to iterate over events in this block.  The
 * event can only be processed once, and it is expected that
 * popEvent will be called until it returns null.
 */
MidiSyncEvent* MidiQueue::popEvent()
{
    MidiSyncEvent* event = nullptr;
    if (eventTail != eventHead) {
		event = &(events[eventTail]);
		eventTail++;
		if (eventTail >= MaxSyncEvents)
		  eventTail = 0;
    }
    return event;
}

/**
 * Initialize an iterator into the event list.  An alternative to popEvent
 * for cases where something needs to iterate over the event list more than once.
 * I forget why this was necessary, I think for the old Synchronizer that injected
 * events for every track and need to iterate once for each track.
 * Pulsator does not need this.
 */
void MidiQueue::iterateStart()
{
    iterateTail = eventTail;
    iterateHead = eventHead;
}

MidiSyncEvent* MidiQueue::iterateNext()
{
    MidiSyncEvent* event = nullptr;
    if (iterateTail != iterateHead) {
		event = &(events[iterateTail]);
		iterateTail++;
		if (iterateTail >= MaxSyncEvents)
		  iterateTail = 0;
    }
    return event;
}    

/**
 * Flush any lingering events in the queue.
 * If you use the iterator interface you MUST call this when
 * all iterations have finished.
 */
void MidiQueue::flushEvents()
{
    eventTail = eventHead;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

