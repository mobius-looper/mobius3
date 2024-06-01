/**
 * Object representing one significant event in the realtime
 * MIDI message stream.
 *
 * Note that there is no isClock flag.  Insignificant clock messages
 * are represented by an event with all flags off.
 *
 * This is part of the model shared between the UI and the engine.
 * They will be created by MidiQueue and held until the next audio interrupt
 * then are exepected to be consumed.
 *
 * The engine gets the next message by calling MobiusMidiTransport::nextInputEvent
 * and nextOutputEvent which is implemented by MidiRealizer.
 */

#pragma once

class MidiSyncEvent {
  public:

    /**
     * The system millisecond counter at the time this event was received.
     */
    int millisecond = 0;

    /**
     * True if this event represents a 0xFC Stop message.
     * This is the only event that does not correspond to a 0xF8 clock message.
     */
    bool isStop = false;

    /**
     * True if this event represents the onset of a Start.
     * This will be the first clock after a 0xFA Start message is received.
     */
    bool isStart = false;

    /**
     * True if this event represents the onset of a Continue.
     * This will be the first clock after a 0xFB Continue message is received.
     */
    bool isContinue = false;

    /**
     * True if this event represents the start of a beat.
     * This will always be true when isStart is also true
     * and will be true whenever 24 clock messages have been received.
     */
    bool isBeat = false;

    /**
     * Raw beat number if isBeat is true
     */
    int beat = 0;

    /**
     * May be non-zero when isContinue is true and holds the value
     * of songClock.  Note that this is NOT the raw SongPosition message value.
     * It was scaled down to a clock "pulse" for Synchronizer.
     */
    int songClock = 0;

    void reset();
    
};

