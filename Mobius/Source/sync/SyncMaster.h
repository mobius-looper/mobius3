/**
 * A major system component that provides services related to synchronization.
 *
 * It is an AudioThread/Kernel component and must be accessed from the UI
 * trough Actions and Querys.
 *
 * Among the services provided are:
 *
 *    - A "Transport" for an internal sync generator with a tempo, time signature
 *      and a timeline that may be advanced with Start/Stop/Pause commands
 *    - Receiption of MIDI input clocks and conversion into an internal Pulse model
 *    - Reception of plugin host synchronization state with Pulse conversion
 *    - Generation of MIDI output clocks to send to external applications and devices
 *    - Generation of an audible Metronome
 *
 */

#pragma once

#include <JuceHeader.h>

#include "SyncMasterState.h"
#include "Pulse.h"

class SyncMaster
{
    friend class Pulsator;
    friend class Transport;
    
  public:

    /**
     * Used by TimeSlicer to know when leaders or followers have changed.
     */
    class Listener {
      public:
        virtual ~Listener() {}
        virtual void syncFollowerChanges() = 0;
    };
    
    SyncMaster();
    ~SyncMaster();

    void addListener(Listener* l);

    void initialize(class MobiusKernel* k);
    void loadSession(class Session* s);
    void shutdown();

    //
    // Block Lifecycle
    //

    void beginAudioBlock(class MobiusAudioStream* stream);
    void advance(class MobiusAudioStream* stream);
    bool doAction(class UIAction* a);
    bool doQuery(class Query* q);
    void refreshState(class SyncMasterState* s);
    void refreshPriorityState(class PriorityState* s);

    //
    // Masters
    //

    void setTrackSyncMaster(int id);
    int getTrackSyncMaster();
    
    void setTransportMaster(int id);
    int getTransportMaster();

    Pulse::Source getEffectiveSource(int id);

    //
    // Track Notifications
    //

    void notifyTrackRecord(int id);
    int notifyTrackRecordEnding(int id);
    void notifyTrackAvailable(int id);
    void notifyTrackReset(int id);
    void notifyTrackRestructure(int id);
    void notifyTrackStart(int id);
    void notifyTrackPause(int id);
    void notifyTrackResume(int id);
    void notifyTrackMute(int id);
    void notifyTrackMove(int id);
    void notifyTrackSpeed(int id);

    // Hopefully temporary support for the old MIDI options
    // that would send START and STOP messages when certain
    // mode transitions happened or were manually scheduled
    // e.g. the MidiStart and MuteMidiStart functions

    void notifyMidiStart(int id);
    void notifyMidiStop(int id);
    
    //
    // Granular state
    // This is temporary for Synchronizer to build
    // the OldMobiusTrackState, once the state model transition
    // is complete, these can be removed
    //
    
    float getTempo(Pulse::Source src);
    int getBeat(Pulse::Source src);
    int getBar(Pulse::Source src);
    int getBeatsPerBar(Pulse::Source src);

    // used by Synchronizer for AutoRecord
    int getBarFrames(Pulse::Source src);
    
    //
    // TimeSlicer Interface
    //

    Pulse* getBlockPulse(class Follower* f);

    //
    // Internal Component Services
    //
    
    int getMilliseconds();
    void sendAlert(juce::String msg);

    //////////////////////////////////////////////////////////////////////
    // Leader/Follower Pulsator passthroughs
    //////////////////////////////////////////////////////////////////////

    // register the intent to follow
    void follow(int follower, Pulse::Source source, Pulse::Type type);
    void follow(int follower, int leader, Pulse::Type type);
    void unfollow(int follower);

    // notify that a leader pulse has been reached
    void addLeaderPulse(int leader, Pulse::Type type, int frameOffset);

    // notify that drift has been corrected
    void correctDrift(int follower, int frames);
    
    // Mostly historical, try to move most of this inside

    class Follower* getFollower(int id);
    void start(int follower);
    void lock(int follower, int frames);
    void unlock(int follower);
    bool shouldCheckDrift(int follower);
    int getDrift(int follower);

    //////////////////////////////////////////////////////////////////////
    // Host State
    //////////////////////////////////////////////////////////////////////

    // It shouldn't be necessary to expose these to the outside
    // Synchronizer uses this to assemble OldMobiusState and there
    // are old core Variables that expose it to MOS scripts

    bool isHostReceiving();
    bool isHostStarted();
    
    //////////////////////////////////////////////////////////////////////
    // Transport/MIDI Output
    //////////////////////////////////////////////////////////////////////

    // Little of this should be necessary
    // Some is used by old Mobius to assemble State

    float getTempo();

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
     * For Synchronizer::getMidiSongClock, not exposed as a variable.
     * Used only for trace messages.
     * Be sure to return the ITERATOR clock, not the global one that hasn't
     * been incremented yet.
     */
    int getMidiOutSongClock();

    //////////////////////////////////////////////////////////////////////
    // MIDI Input
    //////////////////////////////////////////////////////////////////////

    // Also unnecessary after the SystemState transition

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

  protected:

    // Accessors for cross-component usage
    // Could also just pass these during initialization

    class MidiAnalyzer* getMidiAnalyzer() {
        return midiAnalyzer.get();
    }

    class MidiRealizer* getMidiRealizer() {
        return midiRealizer.get();
    }

    class Transport* getTransport() {
        return transport.get();
    }
    
    class Pulsator* getPulsator() {
        return pulsator.get();
    }
    
    class HostAudioTime* getAudioTime();

  private:

    class MobiusKernel* kernel = nullptr;
    class MobiusContainer* container = nullptr;
    Listener* listener = nullptr;
    
    int sampleRate = 44100;
    int trackSyncMaster = 0;
    int transportMaster = 0;

    // MidiAnalyzer and Transport manage their own state
    // Now that HostAnalyzer is here, it could handle this
    SyncSourceState host;
    
    std::unique_ptr<class MidiRealizer> midiRealizer;
    std::unique_ptr<class MidiAnalyzer> midiAnalyzer;
    std::unique_ptr<class HostAnalyzer> hostAnalyzer;
    std::unique_ptr<class HostAnalyzerV2> hostAnalyzer2;
    std::unique_ptr<class Pulsator> pulsator;
    std::unique_ptr<class Transport> transport;
    
    void refreshSampleRate(int rate);
    void enableEventQueue();
    void disableEventQueue();

    void setTrackSyncMaster(class UIAction* a);
    void setTransportMaster(class UIAction* a);
    
    void connectTransport(int id);
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
