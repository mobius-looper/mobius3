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

#include "SyncConstants.h"

class SyncMaster
{
    friend class Transport;
    friend class Pulsator;
    friend class BarTender;
    
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
    void refreshState(class SyncState* s);
    void refreshPriorityState(class PriorityState* s);

    //
    // TimeSlicer Interface
    // This is really the entire reason we exist
    //

    class Pulse* getBlockPulse(int trackNumber);

    //
    // Masters
    //

    void setTrackSyncMaster(int id);
    int getTrackSyncMaster();
    
    void setTransportMaster(int id);
    int getTransportMaster();

    SyncSource getEffectiveSource(int id);

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
    // is complete, these can be removed.
    // Also used by some old script Variables
    //
    
    float getTempo(int number);
    int getBeat(int number);
    int getBar(int number);
    int getBeatsPerBar(int number);
    
    float varGetTempo(SyncSource src);
    int varGetBeat(SyncSource src);
    int varGetBar(SyncSource src);
    int varGetBeatsPerBar(SyncSource src);

    // used by Synchronizer for AutoRecord
    int getBarFrames(SyncSource src);
    
    //
    // Internal Component Services
    //
    
    int getMilliseconds();
    void sendAlert(juce::String msg);

    //////////////////////////////////////////////////////////////////////
    // Leader/Follower Pulsator passthroughs
    //////////////////////////////////////////////////////////////////////

    class Follower* getFollower(int id);
    
    void follow(int follower, SyncSource source, SyncUnit unit);
    void follow(int follower, int leader, SyncUnit unit);
    void unfollow(int follower);

    // notify that a leader pulse has been reached
    void addLeaderPulse(int leader, SyncUnit unit, int frameOffset);

    //////////////////////////////////////////////////////////////////////
    // Host State
    //////////////////////////////////////////////////////////////////////

    // It shouldn't be necessary to expose these to the outside
    // Synchronizer uses this to assemble OldMobiusState and there
    // are old core Variables that expose it to MOS scripts

    bool varIsHostReceiving();
    bool varIsHostStarted();
    
    //////////////////////////////////////////////////////////////////////
    // Transport/MIDI Output
    //////////////////////////////////////////////////////////////////////

    // Little of this should be necessary
    // Some is used by old Mobius to assemble State

    /**
     * Return the raw beat counter.  This will be zero if the clock is not running.
     */
    int varGetMidiOutRawBeat();

    /**
     * True if we're actively sending MIDI clocks.
     */
    bool varIsMidiOutSending();

    /**
     * True if we've sent MIDI Start and are sending clocks.
     * Not sure why we have both, I guess we could have been sending clocks
     * to prepare the receiver, but sent start/stop independently.
     */
    bool varIsMidiOutStarted();

    /**
     * The number of Start messages sent since the last Stop.
     * Old notes say "used by the unit tests to verify we're sending starts".
     */
    int varGetMidiOutStarts();

    /**
     * Old notes:
     * For Synchronizer::getMidiSongClock, not exposed as a variable.
     * Used only for trace messages.
     * Be sure to return the ITERATOR clock, not the global one that hasn't
     * been incremented yet.
     */
    int varGetMidiOutSongClock();

    //////////////////////////////////////////////////////////////////////
    // MIDI Input
    //////////////////////////////////////////////////////////////////////

    // Also unnecessary after the SystemState transition

    /**
     * The raw measured tempo of the incomming clock stream.
     */
    float varGetMidiInTempo();

    /**
     * For display purposes, a filtered tempo that can jitter
     * less than getInputTempo.  This is a 10x integer to remove
     * long floating fractions.
     */
    int varGetMidiInSmoothTempo();

    int varGetMidiInRawBeat();
    int varGetMidiInSongClock();
    bool varIsMidiInReceiving();
    bool varIsMidiInStarted();

  protected:

    // Accessors for cross-component usage
    // Could also just pass these during initialization

    class HostAnalyzer* getHostAnalyzer() {
        return hostAnalyzer.get();
    }

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

    class BarTender* getBarTender() {
        return barTender.get();
    }

    class TrackManager* getTrackManager();

  private:

    class MobiusKernel* kernel = nullptr;
    class MobiusContainer* container = nullptr;
    Listener* listener = nullptr;
    
    int sampleRate = 44100;
    int trackSyncMaster = 0;
    int transportMaster = 0;

    std::unique_ptr<class MidiRealizer> midiRealizer;
    std::unique_ptr<class MidiAnalyzer> midiAnalyzer;
    std::unique_ptr<class HostAnalyzer> hostAnalyzer;
    std::unique_ptr<class Transport> transport;
    std::unique_ptr<class BarTender> barTender;
    std::unique_ptr<class Pulsator> pulsator;
    
    void refreshSampleRate(int rate);
    void enableEventQueue();
    void disableEventQueue();

    void setTrackSyncMaster(class UIAction* a);
    void setTransportMaster(class UIAction* a);
    
    void connectTransport(int id);

    void checkDrifts();
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
