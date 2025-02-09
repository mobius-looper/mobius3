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

#include "../../model/SyncConstants.h"
#include "../../model/SessionHelper.h"

class SyncMaster
{
    friend class Transport;
    friend class Pulsator;
    friend class BarTender;
    friend class MidiAnalyzer;
    friend class TimeSlicer;
    
  public:

    /**
     * Structure returned by the SyncMaster's requestRecordStart
     * or requestRecordStop methods.
     */
    class RequestResult {
      public:
        // true if the recording is expected to be synchronized based
        // on the track's SyncMode 
        bool synchronized = false;
        // unit length if known
        int unitLength = 0;
    };
    
    SyncMaster();
    ~SyncMaster();

    void initialize(class MobiusKernel* k, class TrackManager* tm);
    void loadSession(class Session* s);
    void shutdown();
    void globalReset();

    //
    // Block Lifecycle
    //

    int getBlockCount();
    void beginAudioBlock(class MobiusAudioStream* stream);
    void processAudioStream(class MobiusAudioStream* stream);
    bool doAction(class UIAction* a);
    bool doQuery(class Query* q);
    void refreshState(class SystemState* s);
    void refreshPriorityState(class PriorityState* s);

    //
    // TimeSlicer Interface
    // This is really the entire reason we exist
    //

    class Pulse* getBlockPulse(class LogicalTrack* t);

    //
    // Masters
    //

    void setTrackSyncMaster(int id);
    int getTrackSyncMaster();
    
    void setTransportMaster(int id);
    int getTransportMaster();

    SyncSource getEffectiveSource(int id);
    SyncSource getEffectiveSource(class LogicalTrack* t);
    SyncUnit getSyncUnit(int id);
    int getSyncUnitLength(int id);

    //
    // Sync Recording
    //

    bool isRecordSynchronized(int number);
    bool hasRecordThreshold(int number);
    bool isSyncRecording(int number);
    
    int getRecordThreshold();

    RequestResult requestRecordStart(int number, SyncUnit startUnit, SyncUnit pulseUnit);
    RequestResult requestRecordStart(int number, SyncUnit unit);
    RequestResult requestRecordStart(int number);
    RequestResult requestRecordStop(int number);
    RequestResult requestAutoRecord(int number);
   
    //
    // Track Notifications
    //
    
    void notifyRecordStarted(int id);
    void notifyRecordStopped(int id);
    
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

    //
    // AutoRecord Support
    //

    int getAutoRecordUnitLength(int id);
    int getAutoRecordUnits(int id);

    // used by Synchronizer for AutoRecord
    int getBarFrames(SyncSource src);
    
    //
    // Internal Component Services
    //
    
    void sendAlert(juce::String msg);

    // notify that a leader pulse has been reached
    void addLeaderPulse(int leader, SyncUnit unit, int frameOffset);

    int getActiveFollowers(SyncSource src, int unitLength);

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

    class SymbolTable* getSymbols();

    void handlePulseResult(class LogicalTrack* track, bool ended);
    void notifyTransportStarted();
    
  private:

    class MobiusKernel* kernel = nullptr;
    class MobiusContainer* container = nullptr;
    class TrackManager* trackManager = nullptr;

    int sampleRate = 44100;
    int blockCount = 0;
    int trackSyncMaster = 0;

    SessionHelper sessionHelper;

    // cached session parameters
    bool manualStart = false;
    SyncUnit autoRecordUnit = SyncUnitBar;
    int autoRecordUnits = 1;
    int recordThreshold = 0;
    
    std::unique_ptr<class MidiRealizer> midiRealizer;
    std::unique_ptr<class MidiAnalyzer> midiAnalyzer;
    std::unique_ptr<class HostAnalyzer> hostAnalyzer;
    std::unique_ptr<class Transport> transport;
    std::unique_ptr<class BarTender> barTender;
    std::unique_ptr<class Pulsator> pulsator;
    std::unique_ptr<class TimeSlicer> timeSlicer;
    
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
