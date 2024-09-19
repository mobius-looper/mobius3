/**
 * The interface of an object that provides MIDI synchronization
 * services to the engine.  This could be considered part of the
 * MobiusContainer interface but is relatively limited in functionality
 * and usage and more likely to evolve.
 *
 * Under Juce, this will be implemented with standard Juce MIDI devices
 * and a high resolution thread for clock timing.  It combines aspects of
 * both input and output clock synchronization which makes it less of a 
 * pure "transport" but the code is all closely related and makes the
 * integration with the engine Synchronizer simpler.
 */

#pragma once

class MobiusMidiTransport
{
  public:

    virtual ~MobiusMidiTransport() {}

    //////////////////////////////////////////////////////////////////////
    // Output Sync
    //////////////////////////////////////////////////////////////////////

    /**
     * Return the current tempo used when sending MIDI clocks.
     */
    virtual float getTempo() = 0;
    virtual void setTempo(float tempo) = 0;

    /**
     * Return the raw beat counter.  This will be zero if the clock is not running.
     */
    virtual int getRawBeat() = 0;

    /**
     * True if we're actively sending MIDI clocks.
     */
    virtual bool isSending() = 0;

    /**
     * True if we've sent MIDI Start and are sending clocks.
     * Not sure why we have both, I guess we could have been sending clocks
     * to prepare the receiver, but sent start/stop independently.
     */
    virtual bool isStarted() = 0;

    /**
     * The number of Start messages sent since the last Stop.
     * Old notes say "used by the unit tests to verify we're sending starts".
     */
    virtual int getStarts() = 0;

    /**
     * Old notes:
     * The unit tests want to verify that we at least tried
     * to send a start event.  If we suppressed one because we're
     * already there, still increment the start count.
     */
    virtual void incStarts() = 0;

    /**
     * Old notes:
     * For Synchronizer::getMidiSongClock, not exposed as a variable.
     * Used only for trace messages.
     * Be sure to return the ITERATOR clock, not the global one that hasn't
     * been incremented yet.
     */
    virtual int getSongClock() = 0;

    /**
     * Send a Start message and start sending clocks if we aren't already.
     */
    virtual void start() = 0;

    /**
     * Start sending clocks if we aren't already, but don't send a Start message.
     */
    virtual void startClocks() = 0;

    /**
     * Send a Stop message and stop sending clocks.
     */
    virtual void stop() =  0;
    
    /**
     * Send a combination of Stop message and clocks.
     * Old notes: 
     * After entering Mute or Pause modes, decide whether to send
     * MIDI transport commands and stop clocks.  This is controlled
     * by an obscure option MuteSyncMode.  This is for dumb devices
     * that don't understand STOP/START/CONTINUE messages.
     *
     * Don't know if we still need this, but keep it for awhile.
     */
    virtual void stopSelective(bool sendStop, bool stopClocks) = 0;

    /**
     * Send a Continue message and start sending clocks.
     */
    virtual void midiContinue() = 0;

    virtual class MidiSyncEvent* nextOutputEvent() = 0;
    
    //////////////////////////////////////////////////////////////////////
    // Input Sync
    //////////////////////////////////////////////////////////////////////
    
    virtual class MidiSyncEvent* nextInputEvent() = 0;
    
    /**
     * An accurate millisecond counter provided by the container.
     * We have this in MobiusContainer as well, but Synchronizer has
     * historically expected it here so duplicate for now.
     */
    virtual int getMilliseconds() = 0;

    /**
     * The raw measured tempo of the incomming clock stream.
     */
    virtual float getInputTempo() = 0;

    /**
     * For display purposes, a filtered tempo that can jitter
     * less than getInputTempo.  This is a 10x integer to remove
     * long floating fractions.
     */
    virtual int getInputSmoothTempo() = 0;

    virtual int getInputRawBeat() = 0;
    virtual int getInputSongClock() = 0;
    virtual bool isInputReceiving() = 0;
    virtual bool isInputStarted() = 0;
    
};

