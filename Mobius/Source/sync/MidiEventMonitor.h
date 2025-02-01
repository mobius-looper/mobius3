/**
 * Small utility that implements the basic algorithm for MIDI realtime event stream
 * monitoring and detecting state transitions.
 *
 * Analysis of clock tempo is performed by MidiTempoMonitor
 *
 */

#pragma once

class MidiEventMonitor
{
  public:

    MidiEventMonitor() {}
    ~MidiEventMonitor() {}

    /**
     * Consume a received message and adjust internal state.
     */
    void consume(const juce::MidiMessage& msg);

    /**
     * Reset all analysis results.
     * This may be done when disruptions happen in the MIDI stream such as
     * changing the monitored device, or detecting that clocks have stopped.
     */
    void reset();

    //
    // Analysis results
    //

    /**
     * True if we are in a Started state.
     *
     * This becomes true after recipt of a 0xFA Start message
     * or a 0xFB continue, AND the recipt of the first clock after those
     * events.
     *
     * It will become false after recipt of a 0xFA Stop message.
     */
    bool started = false;

    /**
     * True if the previous Started transition was the result
     * of a 0xFB continue message.
     */
    bool continued = false;

    /**
     * The current Song Position Pointer.
     *
     * This is set to zero after recipt of a Start message or
     * set to a non-zero value after recipt of a 0xF2 SongPosition
     * message.
     *
     * Once in a started state, it will increment by 1 after every 6 clocks
     * have been received.
     */
    int songPosition = 0;

    /**
     * The native beat number.
     *
     * This starts at zero after a Start message and increments by 1 after
     * every 24 clocks have been received.  It may jump to non-sequential value
     * after receipt of a SongPosition message.
     *
     * Note that the name "beat" means what MIDI calls the "quarter note".
     * In the standard, a "beat" corresponds to a sixteenth note which are the
     * units of the Song Position Pointer.
     */
    int beat = 0;

    /**
     * The elapsed beat number after starting.
     */
    int elapsedBeats = 0;

    /**
     * The elapsed clock count since the last Start or Continue
     */
    int clock = 0;

  private:

    // true after recipt of Start while waiting for the next clock
    bool startPending = false;

    // true after recipt of Continue while waiting for the next clock
    bool continuePending = false;

    // the number of clocks that have elapsed since the last song position boundary
    int songUnitClock = 0;

    // the number of clocks that have elapsed since the last beat boundary
    int beatClock = 0;

};

    
