/**
 * Utility class used to accumulate MIDI realtime messages,
 * and convert them to a simplified model closer to what the
 * Mobius engine wants to process.
 */

#pragma once

#include "MidiSyncEvent.h"

/**
 * Object maintaining a list of MidiSyncEvents that have been
 * received and the active state of the realtime MIDI message stream.
 */
class MidiQueue
{
  public:

    /**
     * Maximum number of MidiSyncEvents we can hold.
     * This will be filled by the MIDI device thread as events come in,
     * and is expected to be consumed at regular intervals,
     * typically in the audio thread for the plugin to process.
     */
    static const int MaxSyncEvents = 128;

    MidiQueue();
    ~MidiQueue();

	/**
	 * Queues may be given a name for internal trace messages.
     * In current use this will be "internal" or "external".
	 */
	void setName(const char* name);

    /**
     * Execpted to be called at regular intervals with the current
     * system millisecond counter.  Used to detect clock stoppage.
     */
    void checkClocks(int millisecond);

    /**
     * Enable/disable the accumulation of MidiSyncEvents.
     * We will still track logical state, but not create events.
     */
    void setEnableEvents(bool enable);

    /**
     * Remember the song position from a SongPosition message.
     */
    void setSongPosition(int songpos);

    /**
     * Accumulate a realtime message: Start/Stop/Continue/Clock.
     */
    void add(int status, int millisecond);

    /**
     * True if there are events waiting.
     */
    bool hasEvents();

    /**
     * Remove and return the next event in the queue.
     * Ownership is not transferred.
     * This is the original interface used by core/Synchronizer
     */
    MidiSyncEvent* popEvent();

    /**
     * Start iterating over the event list without popping them.
     * This is the new interface used by Pulsator.  Iteration
     * state is maintained internall, so there can only be one
     * iteration happening at a time.
     */
    void iterateStart();

    /**
     * Return the next event in the queue without popping
     */
    MidiSyncEvent* iterateNext();

    /**
     * Release accumulated dangling events at the end of the interrupt.
     */
    void flushEvents();

    /**
	 * True if clocks are comming in often enough for us to consider
	 * that a device is connected and active.
	 */
	bool receivingClocks = false;

	/**
	 * True if we've entered a start state after receiving either
	 * a 0xFA Start or 0xFB Continue message, and consuming the
     * 0xF8 Clock message immediately following.
	 */ 
	bool started = false;

	/**
	 * Incremented whenever beatClocks reaches 24.
	 * The beat counter increments without bound since the notion
	 * of a "bar" is a higher level concept that can change at any time.
	 */
	int beat = 0;

	/**
	 * Number of MIDI clocks within the "song".  This is set to zero
	 * after an MS_START, or derived from songPosition after an
	 * MS_CONTINUE.  It then increments without bound.
	 */
	int songClock = 0;

    void setTraceEnabled(bool b) {
        traceEnabled = b;
    }
    
  private:

    MidiSyncEvent* reserveEvent();
    void commitEvent();

    const char* queueName = nullptr;

    /**
     * The "list" of sync events managed with a ring buffer.
     */
	MidiSyncEvent events[MaxSyncEvents];

    /**
     * Index into the event list where new events are placed.
     */
	int eventHead = 0;
    int iterateHead = 0;
    
    /**
     * Index into the event list where old events are consumed.
     */
	int eventTail = 0;
    int iterateTail = 0;

    /**
     * Number of events we were unable to save due to buffer overflow.
     */
	int eventOverflows = 0;

    /**
     * Testing flag to turn event generation on and off.
     */
    bool enableEvents = false;
    
    /**
	 * The millisecond timestamp of the last 0xF8 Clock message.
	 * Used to measure the distance between clocks to see if the clock
	 * stream has started or stopped.
	 */
	int lastClockMillisecond = 0;

	/**
	 * The status byte of the last MIDI event that requires that we wait
	 * until the next clock to activate.  This will be either MS_START
	 * or MS_CONTINUE.   This is cleared immediately after receiving
	 * the next MS_CLOCK *after* the one that caused us to start.
	 */
	int waitingStatus = 0;

	/**
	 * Set after receiving a MS_SONGPOSITION event.
	 * We don't change position immediately, but save it for the
	 * next MS_CONTINUE event.  This is not saved after MS_CONTINUE,
     * it is converted to songClock and set back to -1.
     * 
	 */
	int songPosition = -1;

	/**
	 * This starts at zero and counts up to 24, then rolls back to zero.
	 * When it reaches 24, the "beat" field is incremented.
	 * This is It is recalcualted whenever songClock changes.
	 */
	int beatClock = 0;


    bool traceEnabled = false;

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
