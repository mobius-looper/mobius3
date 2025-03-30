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

#include "../../model/ParameterConstants.h"
#include "../../model/SyncConstants.h"
#include "../../model/SessionHelper.h"
#include "SyncEvent.h"
#include "Pulse.h"

class SyncMaster
{
    friend class Transport;
    friend class Pulsator;
    friend class BarTender;
    friend class Unitarian;
    friend class MidiAnalyzer;
    friend class TimeSlicer;
    
  public:

    /**
     * Structure returned by the SyncMaster's requestRecordStart
     * or requestRecordStop methods.
     *
     * Originally thought I needed to return the unitLength here but
     * that's handled in a better way now, don't really need this.
     */
    class RequestResult {
      public:
        // true if the recording is expected to be synchronized based
        // on the track's SyncMode 
        bool synchronized = false;

        // extra details for requestAutoRecord
        int autoRecordUnits = 0;
        int autoRecordLength = 0;

        // non-zero if there is a record threshold
        int threshold = 0;

        // new goal units after an extension or reduction
        int goalUnits = 0;
        int extensionLength = 0;
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

    class LogicalTrack* getLeaderTrack(class LogicalTrack* follower);
    
    //
    // Sync Recording Requests
    //

    bool isSyncRecording(int number);
    
    int getRecordThreshold();

    RequestResult requestRecordStart(int number, SyncUnit pulseUnit, SyncUnit startUnit, bool noSync);
    RequestResult requestRecordStart(int number, SyncUnit unit, bool noSync);
    RequestResult requestRecordStart(int number, bool noSync);
    RequestResult requestSwitchRecord(int number, int blockOffset);
    RequestResult requestRecordStop(int number, bool noSync);
    RequestResult requestPreRecordStop(int number);
    RequestResult requestAutoRecord(int number, bool noSync);
    RequestResult requestExtension(int number);
    RequestResult requestReduction(int number);
   
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
    // Internal Component Services
    //
    
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

    class SymbolTable* getSymbols();

    void handleBlockPulse(class LogicalTrack* track, class Pulse* pulse);

    // Special support for Transport pulses that come in after the first advance
    int getBlockOffset();
    int getBlockSize();
    void notifyTransportStarted();
    
  private:

    class MobiusKernel* kernel = nullptr;
    class MobiusContainer* container = nullptr;
    class TrackManager* trackManager = nullptr;

    int sampleRate = 44100;
    int blockCount = 0;
    int blockSize = 0;
    int trackSyncMaster = 0;
    TrackMasterReset trackMasterReset = TrackMasterStay;
    
    SessionHelper sessionHelper;

    // cached session parameters
    int autoRecordUnits = 1;
    int recordThreshold = 0;

    // pulse holder for internally generated unit pulses
    Pulse unitPulse;
    
    std::unique_ptr<class MidiRealizer> midiRealizer;
    std::unique_ptr<class MidiAnalyzer> midiAnalyzer;
    std::unique_ptr<class HostAnalyzer> hostAnalyzer;
    std::unique_ptr<class Transport> transport;
    std::unique_ptr<class BarTender> barTender;
    std::unique_ptr<class Unitarian> unitarian;
    std::unique_ptr<class Pulsator> pulsator;
    std::unique_ptr<class TimeSlicer> timeSlicer;
    
    void refreshSampleRate(int rate);
    void enableEventQueue();
    void disableEventQueue();

    void setTrackSyncMaster(class UIAction* a);
    void setTransportMaster(class UIAction* a);
    void assignTrackSyncMaster(int former);
    
    void connectTransport(int id);

    void checkDrifts();

    // sync recording internals
    
    void gatherSyncUnits(class LogicalTrack* lt, SyncSource src,
                         SyncUnit recordUnit, SyncUnit startUnit);

    int getAutoRecordUnits(class LogicalTrack* t);
    void lockUnitLength(class LogicalTrack* lt);

    //bool isRecordSynchronized(int number);
    //bool hasRecordThreshold(int number);

    // pulse injection internals

    bool isRelevant(class Pulse* p, SyncUnit unit);
    void sendSyncEvent(class LogicalTrack* t, Pulse* p, SyncEvent::Type type);
    void dealWithSyncEvent(class LogicalTrack* lt, class SyncEvent* event);
    
    void doContortedMidiShit(class LogicalTrack* track, class Pulse* pulse);
    int getGoalBeats(class LogicalTrack* t);
    bool isSourceLocked(class LogicalTrack* t);

    bool extremeTrace = false;
    void traceEvent(class LogicalTrack* t, Pulse* p, SyncEvent& e);
    void tracePulse(class LogicalTrack* t, class Pulse* p);
    int getSyncPlayHead(class LogicalTrack* t);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
