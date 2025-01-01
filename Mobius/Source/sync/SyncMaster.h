/**
 * A major system component that provides services related to synchronization.
 * This is a new and evolving refactoring of parts of the old mobius/core/Synchronizer.
 *
 * It is an AudioThread aka Kernel component and must be accessed from the UI
 * trough Actions.
 *
 * Among the services provided are:
 *
 *    - a "Transport" for an internal sync generator with a tempo, time signature
 *      and a timeline that may be advanced with Start/Stop/Pause commands
 *    - receiption of MIDI input clocks and conversion into an internal Pulse model
 *    - reception of plugin host synchronization state with Pulse conversion
 *    - generation of MIDI output clocks to send to external applications and devices
 *    - generation of an audible Metronome
 *
 * Evolution
 *
 * Just get everything under SyncMaster and redesign...this is far to twisty
 *
  */

#pragma once

#include "SyncMasterState.h"
#include "Transport.h"
#include "Pulse.h"

class SyncMaster
{
  public:
    
    SyncMaster();
    ~SyncMaster();

    void shutdown();
    
    void kludgeSetup(class MobiusKernel* k, class MidiManager* mm);
    void setSampleRate(int rate);

    void enableEventQueue();
    void disableEventQueue();
    
    // needed by MidiRealizer to send to Supervisor
    void sendAlert(juce::String msg);
    
    void loadSession(class Session* s);
    
    void doAction(class UIAction* a);
    bool doQuery(class Query* q);
    void advance(class MobiusAudioStream* stream);

    void refreshState(class SyncMasterState* s);
    void refreshPriorityState(class PriorityState* s);

    // direct access for Synchronizer
    Transport* getTransport() {
        return &transport;
    }

    // this is what core Synchronizer uses to get internal sync pulses
    void getTransportPulse(class Pulse& p);

    /**
     * An accurate millisecond counter provided by the container.
     * We have this in MobiusContainer as well, but Synchronizer has
     * historically expected it here so duplicate for now.
     */
    int getMilliseconds();

    //////////////////////////////////////////////////////////////////////
    // Leader/Follower Pulsator passthroughs
    //////////////////////////////////////////////////////////////////////

    void addLeaderPulse(int leader, Pulse::Type type, int frameOffset);
    void follow(int follower, Pulse::Source source, Pulse::Type type);
    void follow(int follower, int leader, Pulse::Type type);
    void start(int follower);
    void lock(int follower, int frames);
    void unlock(int follower);
    void unfollow(int follower);
    void setOutSyncMaster(int leaderId, int leaderFrames);
    int getOutSyncMaster();
    void setTrackSyncMaster(int leader, int leaderFrames);
    int getTrackSyncMaster();
    bool shouldCheckDrift(int follower);
    int getDrift(int follower);
    void correctDrift(int follower, int frames);
    int getPulseFrame(int follower);
    int getPulseFrame(int followerId, Pulse::Type type);


    // don't need these in Pulsator any more
    float getTempo(Pulse::Source src);
    int getBeat(Pulse::Source src);
    int getBar(Pulse::Source src);
    int getBeatsPerBar(Pulse::Source src);
    
    //////////////////////////////////////////////////////////////////////
    // Transport/MIDI Output
    //////////////////////////////////////////////////////////////////////

    float getTempo();
    void setTempo(float tempo);

    /**
     * Return the raw beat counter.  This will be zero if the clock is not running.
     */
    int getMidiOutRawBeat();

    /**
     * True if we're actively sending MIDI clocks.
     */
    bool isMidiOutSending();

    /**
     * True if we've sent MIDI Start and are sending clocks.
     * Not sure why we have both, I guess we could have been sending clocks
     * to prepare the receiver, but sent start/stop independently.
     */
    bool isMidiOutStarted();

    /**
     * The number of Start messages sent since the last Stop.
     * Old notes say "used by the unit tests to verify we're sending starts".
     */
    int getMidiOutStarts();

    /**
     * Old notes:
     * The unit tests want to verify that we at least tried
     * to send a start event.  If we suppressed one because we're
     * already there, still increment the start count.
     */
    void incMidiOutStarts();

    /**
     * Old notes:
     * For Synchronizer::getMidiSongClock, not exposed as a variable.
     * Used only for trace messages.
     * Be sure to return the ITERATOR clock, not the global one that hasn't
     * been incremented yet.
     */
    int getMidiOutSongClock();

    /**
     * Send a Start message and start sending clocks if we aren't already.
     */
    void midiOutStart();

    /**
     * Start sending clocks if we aren't already, but don't send a Start message.
     */
    void midiOutStartClocks();

    /**
     * Send a Stop message and stop sending clocks.
     */
    void midiOutStop();
    
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
    void midiOutStopSelective(bool sendStop, bool stopClocks);

    /**
     * Send a Continue message and start sending clocks.
     */
    void midiOutContinue();

    // this is used by Synchronizer
    class MidiSyncEvent* midiOutNextEvent();

    // these are used by Pulsator
    void midiOutIterateStart();
    class MidiSyncEvent* midiOutIterateNext();
     
    //////////////////////////////////////////////////////////////////////
    // MIDI Input
    //////////////////////////////////////////////////////////////////////
    
    class MidiSyncEvent* midiInNextEvent();
    void midiInIterateStart();
    class MidiSyncEvent* midiInIterateNext();
    
    /**
     * The raw measured tempo of the incomming clock stream.
     */
    float getMidiInTempo();

    /**
     * For display purposes, a filtered tempo that can jitter
     * less than getInputTempo.  This is a 10x integer to remove
     * long floating fractions.
     */
    int getMidiInSmoothTempo();

    int getMidiInRawBeat();
    int getMidiInSongClock();
    bool isMidiInReceiving();
    bool isMidiInStarted();

  private:

    class MobiusKernel* kernel = nullptr;
    
    SyncMasterState state;
    Transport transport;
    // why can't this be in Transport?
    Pulse transportPulse;
    
    std::unique_ptr<class MidiRealizer> midiRealizer;
    std::unique_ptr<class MidiAnalyzer> midiAnalyzer;
    std::unique_ptr<class Pulsator> pulsator;

    void doStop(class UIAction* a);
    void doStart(class UIAction* a);
    void doTempo(class UIAction* a);
    void doBeatsPerBar(class UIAction* a);

};
