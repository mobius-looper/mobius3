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
#include "../model/SessionHelper.h"

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

    void initialize(class MobiusKernel* k, class TrackManager* tm);
    void loadSession(class Session* s);
    void shutdown();

    //
    // Block Lifecycle
    //

    void beginAudioBlock(class MobiusAudioStream* stream);
    void advance(class MobiusAudioStream* stream);
    bool doAction(class UIAction* a);
    bool doQuery(class Query* q);
    void refreshState(class SystemState* s);
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
    void notifyTrackRestart(int id);
    void notifyTrackPause(int id);
    void notifyTrackResume(int id);
    void notifyTrackMute(int id);
    void notifyTrackMove(int id);
    void notifyTrackSpeed(int id);

    // notifications of user actions that explicitly requested
    // start or stop to be sent
    void notifyMidiStart(int id);
    void notifyMidiStop(int id);

    // used by Synchronizer for AutoRecord
    int getBarFrames(SyncSource src);
    
    //
    // Internal Component Services
    //
    
    int getMilliseconds();
    void sendAlert(juce::String msg);

    // notify that a leader pulse has been reached
    void addLeaderPulse(int leader, SyncUnit unit, int frameOffset);

    //////////////////////////////////////////////////////////////////////
    //
    // Old Variable Support
    // These are for old core script variables.  We don't necessarily need
    // to support these any more, I don't think many if any user scripts
    // used these
    //
    //////////////////////////////////////////////////////////////////////
    
    float varGetTempo(SyncSource src);
    int varGetBeat(SyncSource src);
    int varGetBar(SyncSource src);
    int varGetBeatsPerBar(SyncSource src);

    bool varIsMidiOutSending();
    bool varIsMidiOutStarted();
    
    int varGetMidiInRawBeat();
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
    class TrackManager* trackManager = nullptr;

    // this is dumb, reorganize
    Listener* listener = nullptr;
    
    int sampleRate = 44100;
    int trackSyncMaster = 0;

    SessionHelper sessionHelper;

    // cached session parameters
    bool manualStart = false;

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
