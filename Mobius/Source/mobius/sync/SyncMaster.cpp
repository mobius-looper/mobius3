
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/SystemState.h"
#include "../../model/SyncState.h"
#include "../../model/TrackState.h"
#include "../../model/PriorityState.h"
#include "../../model/Enumerator.h"

// for some of the old sync related modes
#include "../../model/ParameterConstants.h"

#include "../MobiusKernel.h"
#include "../track/TrackManager.h"
#include "../track/LogicalTrack.h"
#include "../track/MslTrack.h"
#include "../track/TrackProperties.h"

#include "Pulsator.h"
#include "Pulse.h"
#include "MidiRealizer.h"
#include "MidiAnalyzer.h"
#include "HostAnalyzer.h"
#include "Transport.h"
#include "Unitarian.h"
#include "TimeSlicer.h"

#include "SyncMaster.h"

SyncMaster::SyncMaster()
{
}

SyncMaster::~SyncMaster()
{
}

/**
 * The MobiusContainer is necessary for these things:
 *   - access to MidiManager
 *   - access to sendAlert()
 *   - access to the sampleRate
 *
 * MidiAnalyzer needs MidiManager to register the realtime event listener.
 * MidiRealizer needs MidiManager to send clock events.
 *
 * MidiRealizer uses sendAlert() to send a few warning messages to the user related
 * to midi device configuration.
 *
 * MidiRealizer and Transport use the SampleRate for some timing calculations.
 *
 * In time, try to factor out a more focused SyncContainer that hides all the other
 * dependencies MobiusContainer drags in.  The MidiRealtimeListener will be a problem
 * since that would have to move out of MidiManager.
 */
void SyncMaster::initialize(MobiusKernel* k, TrackManager* tm)
{
    kernel = k;
    trackManager = tm;
    container = kernel->getContainer();
    sessionHelper.setSymbols(container->getSymbols());

    // these are now dynamically allocated to reduce header file dependencies
    midiRealizer.reset(new MidiRealizer());
    midiAnalyzer.reset(new MidiAnalyzer());
    hostAnalyzer.reset(new HostAnalyzer());
    transport.reset(new Transport(this));

    barTender.reset(new BarTender(this, trackManager));
    unitarian.reset(new Unitarian(this));
    pulsator.reset(new Pulsator(this, trackManager, barTender.get()));

    // reach out and touch the face of god
    hostAnalyzer->initialize(container->getAudioProcessor(), container->getRoot());

    MidiManager* mm = container->getMidiManager();
    midiRealizer->initialize(this, mm);
    midiAnalyzer->initialize(this, mm);

    timeSlicer.reset(new TimeSlicer(this, trackManager));

    // start everything off with a default sample rate, but this
    // may change as soon as the audio devices are open
    refreshSampleRate(44100);
}

/**
 * Pulsator needs this for a few things
 * Transport should be using it for the starting tempo, and various options.
 */
void SyncMaster::loadSession(Session* s)
{
    barTender->loadSession(s);
    pulsator->loadSession(s);
    transport->loadSession(s);
    timeSlicer->loadSession(s);

    // cached parameters
    // !! these are not reset on GlobalReset, probably should be
    // for consistency with everything else
    
    autoRecordUnits = sessionHelper.getInt(s, ParamAutoRecordUnits);
    recordThreshold = sessionHelper.getInt(s, ParamRecordThreshold);

    // trackSyncMaster and transportMaster could also be session parameters
    // but they'r really more transient performance-oriented parameters

    trackMasterReset = (TrackMasterReset)Enumerator::getOrdinal(container->getSymbols(),
                                                                ParamTrackMasterReset,
                                                                s->ensureGlobals(),
                                                                TrackMasterStay);
}

/**
 * Called during the Supervisor::shutdown process.  Make sure the
 * clock generator thread is cleanly stopped.
 */
void SyncMaster::shutdown()
{
    midiRealizer->shutdown();
    midiAnalyzer->shutdown();
}

/**
 * Here when a FuncGlobalReset action is intercepted.
 */
void SyncMaster::globalReset()
{
    transport->globalReset();
    barTender->globalReset();
    midiAnalyzer->globalReset();

    // host analyzer doesn't reset, it continues monitoring the host
    // same with MIDI
}

//////////////////////////////////////////////////////////////////////
//
// Masters
//
//////////////////////////////////////////////////////////////////////

/**
 * There can be one TrackSyncMaster.
 *
 * This becomes the default leader track when using SyncSourceLeader
 * and the follower didn't specify a specific leader.
 * 
 * It may be changed at any time.
 */
void SyncMaster::setTrackSyncMaster(int leaderId)
{
    int oldMaster = trackSyncMaster;
    trackSyncMaster = leaderId;

    // changing this may result in reordering of tracks
    // during an advance
    if (oldMaster != trackSyncMaster) {
        timeSlicer->syncFollowerChanges();
    }
}

int SyncMaster::getTrackSyncMaster()
{
    return trackSyncMaster;
}

/**
 * Action handler for FuncSyncMasterTrack and ParamTrackSyncMaster
 *
 * The way functions are handled went through a few iterations, I started
 * specifying the track as an argument, but this is redundant if you allow
 * it to be given a scope.   This could be implemented in two ways:
 *
 *   Scope
 *     The function is "sent" to a specific track number.  It actually doesn't
 *     get to the LogicalTrack, we intercept it as if it were a global and handle it.
 *
 *   Argument
 *     The function is treated more like an actual global function and logically
 *     not sent to the track.
 *
 * The problem with using Scope is that the scope selector in the binding window
 * will include track groups and it makes no sense to replicate this to a group.
 * But it "feels" right to the user, you're sending a command to a track like
 * any other track function.
 *
 * Using an argument is more like how SelectTrack works.  With the new binding window
 * that can be labeled as "Target Track" or "Track To Select".  
 * 
 * todo: This needs to be expanded to accept any form of track identifier.
 *
 * The parameter action does not allow jumping to the focused track with
 * a zero value so it is predictable as a sweepable host parameter. Value
 * zero means "no master".
 *
 * For FuncSyncMasterTrack, this behaves as a toggle, and if you turn it off
 * it is subject to automatic master reassign if configured.
 */
void SyncMaster::setTrackSyncMaster(UIAction* a)
{
    int newMaster = 0;
    bool autoAssign = false;
    
    if (a->symbol->parameterProperties != nullptr) {
        // number is specified as the action value and does not toggle
        newMaster = a->value;
    }
    else {
        int target = a->value;
        if (target == 0)
          target = container->getFocusedTrackIndex() + 1;

        if (target != trackSyncMaster) {
            // turn it on
            newMaster = target;
        }
        else {
            // turn it off
            setTrackSyncMaster(0);
            assignTrackSyncMaster(target);
            autoAssign = true;
        }
    }
    
    if (!autoAssign) {
        if (newMaster == 0)
          setTrackSyncMaster(0);
        else {
            LogicalTrack* lt = trackManager->getLogicalTrack(newMaster);
            if (lt == nullptr)
              Trace(1, "SyncMaster: Invalid track number in TransportMaster action");
            else {
                setTrackSyncMaster(newMaster);
            }
        }
    }
}

/**
 * There can only be one Transport Master.
 * Changing this may change the tempo of geneerated MIDI clocks if the transport
 * has MIDI enabled.
 */
void SyncMaster::setTransportMaster(int id)
{
    if (transport->getMaster() != id) {

        if (id > 0)
          connectTransport(id);
        else {
            // unusual, they are asking to not have a sync master
            // what else should happen here?  Stop it?
            transport->disconnect();
        }
    }
}

/**
 * Connection between a track and the transport is done
 * by giving Transport the TrackProperties.
 */
void SyncMaster::connectTransport(int id)
{
    TrackManager* tm = kernel->getTrackManager();
    TrackProperties props;
    tm->getTrackProperties(id, props);
    transport->connect(props);
}

int SyncMaster::getTransportMaster()
{
    return transport->getMaster();
}

/**
 * Action handler for FuncSyncMasterTransport and ParamTransportMaster.
 *  
 * Formerly implemented as a Mobius core function OutSyncMaster
 * took no arguments and made the active track the "MIDI Master".
 *
 * todo: SyncMasterMidi was the old name, need a better aliasing mechanism
 *
 * For the function, the track number is specified as the action argument and
 * if it is zero it means to make the focused track the master.
 *
 * For the parameter, the track number is also passed in the action value.
 * The parammeter may have the value zero which means "no master".
 */
void SyncMaster::setTransportMaster(UIAction* a)
{
    int number = a->value;
    if (number == 0 && a->symbol->functionProperties != nullptr) {
        // function allows focus selection
        // todo: not liking how track focus is passed around and where it lives
        number = container->getFocusedTrackIndex() + 1;
    }

    if (number == 0)
      setTransportMaster(number);
    else {
        LogicalTrack* lt = trackManager->getLogicalTrack(number);
        if (lt == nullptr)
          Trace(1, "SyncMaster: Invalid track id in TransportMaster action");
        else {
            setTransportMaster(number);
        }
    }
}

/**
 * The usual dance to get the track sync leaader.
 * Put this somewhere to share.
 * Too much bouncing around between SyncMaster and BarTender with
 * the same damn things.
 */
LogicalTrack* SyncMaster::getLeaderTrack(LogicalTrack* follower)
{
    LogicalTrack* leader = nullptr;
    
    SyncSource src = follower->getSyncSource();
    if (src == SyncSourceTrack) {
        
        int leaderNumber = follower->getSyncLeader();
        if (leaderNumber == 0)
          leaderNumber = getTrackSyncMaster();
        
        if (leaderNumber > 0) {
            leader = trackManager->getLogicalTrack(leaderNumber);
            if (leader == nullptr) {
                Trace(1, "SyncMaster::getLeaderTrack Invalid leader number %d",
                      leaderNumber);
            }
        }
    }
    return leader;
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * This must be called very early in the kernel block processing phase.
 * It initializes the subcomponents for the call to processAudioStream() which
 * happens after various things in the kernel, in particular after
 * any action handling which may assign sync sources to tracks.
 *
 * It is important that sync pulses be analyzed BEFORE actions are processed
 * so that the initiation of synchronized recordings has the updated
 * sync state.
 */
void SyncMaster::beginAudioBlock(MobiusAudioStream* stream)
{
    blockCount++;
    
    // monitor changes to the sample rate once the audio device is pumping
    // and adjust internal calculations
    // this should come through MobiusAudioStream, that's where it lives
    int newSampleRate = stream->getSampleRate();
    if (newSampleRate != sampleRate)
      refreshSampleRate(newSampleRate);

    // once we start receiving audio blocks, it is okay to start converting
    // MIDI events into MidiSyncMessages, if you allow event queueing before
    // blocks come in, the queue can overflow
    // !! this is old and needs to go away, no longer used by MidiAnalyzer
    // and MidiRealizer needs to stop
    enableEventQueue();
    
    // tickle the analyzers

    // Detect whether MIDI clocks have stopped comming in
    //
    // Supervisor formerly called this on the maintenance thread
    // interval, since we don't get a performMaintenance ping down here
    // we can check it on every block, the granularity doesn't really matter
    // since it is based off millisecond time advance
    // !! why can't this just be done in analyze() now?
    midiAnalyzer->checkClocks();

    int frames = stream->getInterruptFrames();
    hostAnalyzer->analyze(frames);
    midiAnalyzer->analyze(frames);
    
    // Transport should be controlling this but until it does it is important
    // to get the event queue consumed, Transport can just ask for the Result
    // when it advances
    
    midiRealizer->advance(frames);
    transport->advance(frames);
    barTender->advance(frames);
    pulsator->advance(frames);

    // see commentary about why this is complicated
    // no longer necessary, Transport does it's own drift checking
    //transport->checkDrift(frames);

    // temporary diagnostics
    checkDrifts();

    // make sure this starts zero for any Actions that follow
    timeSlicer->resetBlockOffset();
    blockSize = frames;
}

void SyncMaster::refreshSampleRate(int rate)
{
    sampleRate = rate;
    
    hostAnalyzer->setSampleRate(rate);
    transport->setSampleRate(rate);
    midiRealizer->setSampleRate(rate);
    midiAnalyzer->setSampleRate(rate);
}

/**
 * Here after actions have been performed, events have been scheduled
 * and we're ready to advance the tracks.
 *
 * This process is controlled by TimeSlicer.
 */
void SyncMaster::processAudioStream(MobiusAudioStream* stream)
{
    timeSlicer->processAudioStream(stream);
}

int SyncMaster::getBlockCount()
{
    return blockCount;
}

/**
 * Used by Transport to calculate the unitPlayHead position after
 * a start() happens due to an action after the initial advance.
 */
int SyncMaster::getBlockSize()
{
    return blockSize;
}

/**
 * Used by Transport to calculate the unitPlayHead position after
 * a start() happens due to an action after the initial advance.
 */
int SyncMaster::getBlockOffset()
{
    return timeSlicer->getBlockOffset();
}

/**
 * Called by Transport whenever it starts as the result of an action.
 * Since this happens after Pulsator was advanced in beginAudioBlock, have
 * to ask it to look again.
 */
void SyncMaster::notifyTransportStarted()
{
    pulsator->notifyTransportStarted();
}

/**
 * The event queue should only be enabled once audio blocks
 * start comming in.  If blocks stop then the queue can overflow
 * if there is MIDI being actively received or sent.
 *
 * Block stoppage can't be monitored here, it would need to be done
 * by a higher power, probablky the maintenance thread.
 */
void SyncMaster::enableEventQueue()
{
    midiRealizer->enableEvents();
}

void SyncMaster::disableEventQueue()
{
    midiRealizer->disableEvents();
}

//////////////////////////////////////////////////////////////////////
//
// Shell Requests
//
//////////////////////////////////////////////////////////////////////

bool SyncMaster::doAction(UIAction* a)
{
    bool handled = true;
    
    switch (a->symbol->id) {

        case FuncSyncMasterTrack:
            setTrackSyncMaster(a);
            break;

        case FuncSyncMasterTransport:
            setTransportMaster(a);
            break;

        case ParamAutoRecordUnit:
            // decided not to use this one, it is just defined
            // by SyncUnit
            break;

        case ParamTrackSyncMaster:
            setTrackSyncMaster(a);
            break;

        case ParamTransportMaster:
            setTransportMaster(a);
            break;

        case ParamAutoRecordUnits:
            autoRecordUnits = a->value;
            break;

        case ParamRecordThreshold:
            recordThreshold = a->value;
            break;
            
        default: {
            handled = transport->doAction(a);
            if (!handled)
              handled = barTender->doAction(a);
        }
            break;
    }
    
    return handled;
}

/**
 * We don't seem to have had parameters for trackSyncMaster and outSyncMaster,
 * those were implemented as script variables.  If they were parameters it would
 * make it more usable for host parameter bindings.
 */
bool SyncMaster::doQuery(Query* q)
{
    bool handled = true;

    switch (q->symbol->id) {

        case ParamAutoRecordUnits:
            q->value = autoRecordUnits;
            break;

        case ParamRecordThreshold:
            q->value = recordThreshold;
            break;

        case ParamTrackSyncMaster:
            q->value = trackSyncMaster;
            break;
            
        case ParamTransportMaster:
            q->value = transport->getMaster();
            break;

        default: {
            handled = transport->doQuery(q);
            if (!handled)
              handled = barTender->doQuery(q);
        }
    }
    
    return handled;
}

/**
 * Add state for each sync source.
 * Also handling sync state for each Track since we're in a good position to do that
 * and don't need to bother the BaseTracks with knowing the details.
 */
void SyncMaster::refreshState(SystemState* sysstate)
{
    SyncState* state = &(sysstate->syncState);
    
    state->transportMaster = transport->getMaster();
    state->trackSyncMaster = trackSyncMaster;

    // the MidiSyncElement wants to display normalized beat/bar/loop numbers
    // and this is not track specific
    // !! need to seriously rethink the utility of track-specific BPB and BPL
    // overrides, why can't this just be global?  it only really matters
    // for the initial recording, then it's just for display
    
    // Analyzer fills everything except normalized beats
    midiAnalyzer->refreshState(state);
    state->midiBeat = barTender->getBeat(SyncSourceMidi);
    state->midiBar = barTender->getBar(SyncSourceMidi);
    state->midiLoop = barTender->getLoop(SyncSourceMidi);
    state->midiBeatsPerBar = barTender->getBeatsPerBar(SyncSourceMidi);
    state->midiBarsPerLoop = barTender->getBarsPerLoop(SyncSourceMidi);

    hostAnalyzer->refreshState(state);
    // !! we've got two sets of these now, should have a generic struct
    // of analyzer results and have BarTender fill all of it in
    state->hostBeat = barTender->getBeat(SyncSourceHost);
    state->hostBar = barTender->getBar(SyncSourceHost);
    state->hostLoop = barTender->getLoop(SyncSourceHost);
    state->hostBeatsPerBar = barTender->getBeatsPerBar(SyncSourceHost);
    state->hostBarsPerLoop = barTender->getBarsPerLoop(SyncSourceHost);
    
    // transport maintains all this inside itself because the time signaturek
    // adapts to the connected loop rather than being always controlled from
    // Session parameters
    transport->refreshState(state);

    int totalTracks = trackManager->getTrackCount();
    int maxStates = sysstate->tracks.size();

    if (maxStates < totalTracks) {
        Trace(1, "SyncMaster: Not enough TrackStates for sync state");
        totalTracks = maxStates;
    }
    
    for (int i = 0 ; i < totalTracks ; i++) {
        TrackState* tstate = sysstate->tracks[i];
        int trackNumber = i+1;

        // we have so far been the one to put sync state in SystemState
        // but since this is all on the LogicalTrack now TM could do it

        LogicalTrack* lt = trackManager->getLogicalTrack(trackNumber);
        if (lt != nullptr) {
            tstate->syncSource = lt->getSyncSource();
            tstate->syncUnit = lt->getSyncUnit();
            tstate->trackSyncUnit = lt->getTrackSyncUnit();

            // old convention was to suppress beat/bar display
            // if the source was not in a started state
            bool running = true;
            if (tstate->syncSource == SyncSourceMidi)
              running = midiAnalyzer->isRunning();
            else if (tstate->syncSource == SyncSourceHost)
              running = hostAnalyzer->isRunning();

            // the convention has been that if beat or bar are
            // zero they are undefined and not shown, TempoElement assumes this
            if (running) {
                tstate->syncBeat = barTender->getBeat(lt) + 1;
                tstate->syncBar = barTender->getBar(lt) + 1;
            }
            else {
                tstate->syncBeat = 0;
                tstate->syncBar = 0;
            }
        }
    }
}

void SyncMaster::refreshPriorityState(PriorityState* pstate)
{
    transport->refreshPriorityState(pstate);

    pstate->midiBeat = barTender->getBeat(SyncSourceMidi);
    pstate->midiBar = barTender->getBar(SyncSourceMidi);
    pstate->midiLoop = barTender->getLoop(SyncSourceMidi);

    pstate->hostBeat = barTender->getBeat(SyncSourceHost);
    pstate->hostBar = barTender->getBar(SyncSourceHost);
    pstate->hostLoop = barTender->getLoop(SyncSourceHost);
    
}

//////////////////////////////////////////////////////////////////////
//
// Synchronized Recording Requests
//
// This collection is called by the BaseTrack when it wants to
// begin or end a new recording.  The recording may be synced or unsynced
// and characteristics it should follow are returned in the RequestResult.
//
//////////////////////////////////////////////////////////////////////

SyncSource SyncMaster::getEffectiveSource(int id)
{
    SyncSource source = SyncSourceNone;
    LogicalTrack* t = trackManager->getLogicalTrack(id);
    if (t != nullptr) {
        source = getEffectiveSource(t);
    }
    return source;
}
        
/**
 * Get the effective sync source for a track.
 *
 * There are two complications here:  
 * The complication here is around SourceMaster which is only allowed
 * if there is no other sync master.
 */
SyncSource SyncMaster::getEffectiveSource(LogicalTrack* lt)
{
    SyncSource source = SyncSourceNone;
    if (lt != nullptr) {
        source = lt->getSyncSource();
        if (source == SyncSourceMaster) {
            int transportMaster = transport->getMaster();
            if (transportMaster > 0 && transportMaster != lt->getNumber()) {
                // there is already a transport master, this track
                // reverts to following the transport
                // !! here is where we need an option to fall back to SyncSourceTrack
                // like we used to
                source = SyncSourceTransport;
            }
        }
        else if (source == SyncSourceTrack) {
            // this is relevant only if there is a track sync master
            source = SyncSourceNone;
            if (trackSyncMaster > 0) {
                // yes, but only if it has content?
                // I suppose this could just start waiting until you record something
                // into the current master track but this raises complications...
                LogicalTrack* mlt = trackManager->getLogicalTrack(trackSyncMaster);
                if (mlt != nullptr && mlt != lt)
                  source = SyncSourceTrack;
            }
        }
    }
    return source;
}

/**
 * Track wants to know this when scheduling AutoRecord stop
 */
bool SyncMaster::isSyncRecording(int number)
{
    bool syncing = false;
    LogicalTrack* t = trackManager->getLogicalTrack(number);
    if (t != nullptr) {
        syncing = t->isSyncRecording();
    }
    return syncing;
}

/**
 * Returns true if the start/stop of a recording is synchronized.
 * If this returns true, it will usually be followed immediately
 * by a call to requestRecordStart or requestRecordStop and it is
 * expected that those succeed.  Not liking the dependency, but
 * works well enough.
 */
// this is no longer necessary outside SM
#if 0
bool SyncMaster::isRecordSynchronized(int number)
{
    bool sync = false;
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        SyncSource src = getEffectiveSource(lt);
        sync = (src != SyncSourceNone && src != SyncSourceMaster);
    }
    return sync;
}
#endif

/**
 * This has historically only returned true if the track was not synchronizaing.
 * If you're synchronizing, waiting for a threshold is much less useful since
 * you know when it's going to start and have time to prepare.
 *
 * Still, they could potentially be combined.  The threshold would be necessary
 * to start the process which may then suspend waiting for a sync pulse.  But this
 * 
 * todo: While threshold is useful on the recording of the first loop, it should
 * be disabled for EmptyLoopAction=Record and some other things.
 */
// this is no longer necessary outside SM
#if 0
bool SyncMaster::hasRecordThreshold(int number)
{
	bool threshold = false;
    if (recordThreshold > 0) {
        threshold = !isRecordSynchronized(number);
	}
    return threshold;
}
#endif

/**
 * Called by the track in response to an action to begin the
 * recording process.   This interface provides the most flexibility
 * to control the recording pulses.  Other signatures derive the arguments
 * from session parameters.
 *
 * This does not allow overriding the track's SyncMode from the session.
 * I suppose it could, but it's easy enough to do it with scripts.  The two
 * units may be specified in a script or as action arguments.
 *
 * If the result has the synchronized flag set, the track is expected to
 * schedule an internal event that will be activated on the next startUnit pulse.
 *  
 * The ending of the recording will be quantized to the pulseUnit.
 *
 * a SyncEvent will be sent on each , pulseUnit pulses will be sent to the track
 * to do things like increment cycle counts or other state related to the increasing
 * length of the loop.
 *
 * The recording process may be ended at any time by the track calling
 * requestRecordStop or by the return value of any syncPulse as pulses are
 * sent into the track.
 *
 * todo: reconsider the need for an alternate startUnit
 */
SyncMaster::RequestResult SyncMaster::requestRecordStart(int number,
                                                         SyncUnit recordUnit,
                                                         SyncUnit startUnit,
                                                         bool noSync)
{
    RequestResult result;

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        SyncSource src = getEffectiveSource(lt);

        // LogicalTrack should be doing this too on block start, but we should
        // be in charge of this
        lt->resetSyncState();
        
        if (src == SyncSourceNone || src == SyncSourceMaster || noSync) {
            // return an empty result and let the track sort it out
        }
        else {
            result.synchronized = true;

            gatherSyncUnits(lt, src, recordUnit, startUnit);
            
            lt->setSyncRecording(true);

            lockUnitLength(lt);
        }
    }
    return result;
}

/**
 * This is a variant requestRecordStart that must be used with Record stacked
 * on a Switch.  Historically the recording will start immediately after the Switch
 * even if this is not exactly on a sync pulse.  Depending on TBD options, this could
 * either ask the track to wait for a sync pulse, or allow it to start immediately.
 *
 * COMPLICATION
 * 
 * When we're about to begin a synchronized recording WITHOUT synchronizing the start,
 * the track is going to have a unit advance in this block that needs to be accumulated.
 * Since we weren't in control over detecting the start pulse, we have to work backward
 * from wherever the track is now.
 *
 * Typically this happens after a LoopSwitch, and the switch could have been quantized
 * anywyere.  So with a block of 256, if the switch happens at frame 100, then the
 * unit advance in this block is 156.  The problem is that we don't have any insight
 * into where the track is as far as block consumption goes.  Even knowing the starting
 * frame and the current frame isn't enough because rate shift can cause the track's
 * internal timeline to advance differently than ours.
 *
 * The track must pass in the number of frames remaining in this block.
 * Could also punch another hole in BaseTrack, but this is obscure.
 */
SyncMaster::RequestResult SyncMaster::requestSwitchRecord(int number, int blockRemaining)
{
    RequestResult result = requestRecordStart(number, SyncUnitNone, SyncUnitNone, false);

    if (result.threshold > 0) {
        // ignore threshold in Switch+Record
        Trace(2, "SyncMaster: Ignoring threshold mode in Switch/Record");
        result.threshold = 0;
    }

    if (result.synchronized) {
        // this would ordinally be synchronized, but in this case we're going to
        // let it start immediately and detect cycle pulses using the internal pulses
        // derived from the unit length
        // for a time this was known as "free start" and should be an option fo any
        // recording, just just after a Switch
        result.synchronized = false;
        LogicalTrack* lt = trackManager->getLogicalTrack(number);
        if (lt != nullptr) {
            lt->setSyncRecordStarted(true);
            lt->setSyncRecordFreeStart(true);
            lt->setSyncElapsedFrames(blockRemaining);
        }
    }

    return result;
}

/**
 * Gather the units a synchronized recording is going to wait on.
 * These normally come from the session parameters, but I'm adding the eventual
 * ability for these to be overridden in the action to accomplish something like this:
 *
 *     Record(4) - record 4 default units
 *     Record(4 beat) - record 4 beats
 *     Record(4 beat loop) - record 4 beats starting on a loop
 */
void SyncMaster::gatherSyncUnits(LogicalTrack* lt, SyncSource src,
                                 SyncUnit recordUnit, SyncUnit startUnit)
{
    SyncUnit defaultUnit = SyncUnitBar;
    
    if (src == SyncSourceTrack) {
        TrackSyncUnit tsu = lt->getTrackSyncUnit();
        if (tsu == TrackUnitNone) {
            Trace(1, "SyncMaster: Someone stored TrackUnitNone in the session");
            tsu = TrackUnitLoop;
        }

        // really hating this conversion, assumes enumerations have the same orrder
        defaultUnit = (SyncUnit)tsu;
    }
    else {
        defaultUnit = lt->getSyncUnit();
        if (defaultUnit == SyncUnitNone) {
            Trace(1, "SyncMaster: Someone stored SyncUnitNone in the session");
            defaultUnit = SyncUnitBar;
        }
    }
            
    if (recordUnit == SyncUnitNone)
      recordUnit = defaultUnit;

    if (startUnit == SyncUnitNone)
      startUnit = recordUnit;

    lt->setSyncRecordUnit(recordUnit);
    lt->setSyncStartUnit(startUnit);
}

/**
 * Used when the start and pulse units are the same.
 */
SyncMaster::RequestResult SyncMaster::requestRecordStart(int number,
                                                         SyncUnit unit, bool noSync)
{
    return requestRecordStart(number, unit, unit, noSync);
}

/**
 * Used when the start and pulse units come from session parameters.
 */
SyncMaster::RequestResult SyncMaster::requestRecordStart(int number, bool noSync)
{
    return requestRecordStart(number, SyncUnitNone, SyncUnitNone, noSync);
}

/**
 * This is called when a track responds to an action that triggers
 * the ending of the recording.  The recording normally ends
 * on the next sync pulse whose unit was defined in requestRecordStart.
 * 
 * The important thing this does is lock the SyncAnalyzer, which in practice
 * is only important for MidiAnalyzer if we allowed the recording to start
 * during the warmup period.
 */
SyncMaster::RequestResult SyncMaster::requestRecordStop(int number, bool noSync)
{
    RequestResult result;
    
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        
        SyncSource src = getEffectiveSource(lt);
        if (src == SyncSourceNone || src == SyncSourceMaster || noSync) {
            // return an empty result and let the track figure it out
        }
        else {
            // do deferred unit locking if not already locked
            // the only one that really needs this is MIDI, but go through the motions
            switch (src) {
                case SyncSourceMidi:
                    midiAnalyzer->lock();
                    break;
                case SyncSourceHost:
                    hostAnalyzer->lock();
                    break;
                case SyncSourceTransport:
                    transport->lock();
                    break;
                default: break;
            }
                    
            result.synchronized = true;

            // this is what switches us from sending Extend events
            // to sending the final Stop event
            if (lt->getSyncGoalUnits() > 0) {
                // why would this happen?
                Trace(1, "SyncMaster: Requested RecordStop with existing goal units");
            }
            else {
                // the goal unit is 1 above where we are now since
                // we are already "in" the unit that hasn't finished yet
                // what about scripts running at the extension point?
                //
                // example: Extension boundary is reached elapsed moves from 1 to 2
                // Script resumes immediately after this and requests a stop
                // goal unit will be set to 3
                // !! yes, this can happen once you start interleaving script
                // waits with pulses, it's the "before or after" boundary problem
                // can't happen yet, but when MSL waits get in here will need
                // to deal with it, the SyncEvent may be before or after the wait
                // and the Wait could be waiting for a SyncEvent...issues here
                int goal = lt->getSyncElapsedUnits() + 1;
                Trace(2, "SyncMaster::requestRecordStop setting goal units %d", goal);
                lt->setSyncGoalUnits(goal);
            }
        }
    }

    return result;
}

/**
 * Variant for AutoRecord
 * A bounded recording is being requested so SM knows when it is supposed to end.
 * Not supporting sync unit overrides here yet, that concept needs more thought.
 *
 * !! Need to support AutoRecord stacked on a Switch too and ignore
 * sync on the start.
 */
SyncMaster::RequestResult SyncMaster::requestAutoRecord(int number, bool noSync)
{
    RequestResult result;

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        
        if (lt->isSyncRecording())
          Trace(1, "SyncMaster: Request to start AutoRecord while already in a recording");
        lt->resetSyncState();
        
        result.autoRecordUnits = getAutoRecordUnits(lt);
        int unitLength = unitarian->getSingleAutoRecordUnitLength(lt);
        result.autoRecordLength = unitLength * result.autoRecordUnits;

        Trace(2, "SyncMaster::requestAutoRecord Goal Units %d", result.autoRecordUnits);
        lt->setSyncGoalUnits(result.autoRecordUnits);
        
        SyncSource src = getEffectiveSource(lt);

        if (src != SyncSourceNone && src != SyncSourceMaster && !noSync) {

            result.synchronized = true;

            gatherSyncUnits(lt, src, SyncUnitNone, SyncUnitNone);
            
            lt->setSyncRecording(true);

            lockUnitLength(lt);
        }

        // in both cases, let it know if there is a recording threshold
        // old comments wonder if the noSync option on the Action should
        // disable threshold recording
        // threshold has historically been disabled if we're synchronizing
        // I imagine they could be combined but it gets messy and complicates testing
        if (!noSync && !result.synchronized)
          result.threshold = recordThreshold;
    }
    return result;
}

/**
 * Used when we're stuck in Synchronize or Threshold modes at
 * the beginning of a recording and they press Record again.
 * Similar to an AutoRecord of one unit.
 */
SyncMaster::RequestResult SyncMaster::requestPreRecordStop(int number)
{
    RequestResult result;

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        // whether synced or unsynced return the length
        result.autoRecordUnits = 1;
        result.autoRecordLength = unitarian->getSingleAutoRecordUnitLength(lt);
        Trace(2, "SyncMaster:requestPreRecordStop: Goal Units 1");
        lt->setSyncGoalUnits(1);

        SyncSource src = getEffectiveSource(lt);
        if (src != SyncSourceNone && src != SyncSourceMaster) {
            result.synchronized = true;
        }
    }
    return result;
}

/**
 * Return the number of units to include in an AutoRecord.
 * The length of each unit is defined by getAutoRecordUnitLength
 *
 * The two are normally multiplied together to get the total length
 * with the autoRecordUnits number becomming the number of cycles in the loop.
 *
 * The value comes from the session.
 */
int SyncMaster::getAutoRecordUnits(LogicalTrack* track)
{
    // this one is not senstiive to the syncSource
    (void)track;
    
    if (autoRecordUnits <= 0) {
         Trace(1, "SyncMaster: Misconfigured autoRecordUnits");
        autoRecordUnits = 1;
    }

    return autoRecordUnits;
}

/**
 * When a recording starts or ends, save the most fundamental unit length
 * of the sync source on the LogicalTrack.  This is used for a few things,
 * particularly with SyncSourceMidi.
 *
 * When a MIDI recording starts we compare the unit length when it started
 * to when it ended to see if any adjustments in the final beat pulse need
 * to be made to keep it aligned.
 *
 * After recording ends, this is used to determine whether the track has
 * a length that is still compatible with the source and should do
 * drift correction.
 *
 * todo: still need some thought here
 */
void SyncMaster::lockUnitLength(LogicalTrack* track)
{
    track->setFundamentalUnitLength(unitarian->getLockUnitLength(track));
}

SyncMaster::RequestResult SyncMaster::requestExtension(int number)
{
    RequestResult result;
    
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        // the number of units to extend, it will always be at least 1
        // for AutoRecord it could be the number of configured units, but
        // I'm thinking just keep it 1 for more fine control
        // if we wanted to have different extension amounts for AutoRecord
        // would need to pass in a flag since we don't know the function that
        // cause this here
        int extension = 1;

        int current = lt->getSyncGoalUnits();
        if (current == 0) {
            // this must be the first extension after ending a recording
            current = 1;
            //Trace(2, "SM: First extension, bumping Goal Unit to 1");
        }

        result.goalUnits = current + extension;

        Trace(2, "SyncMaster::requestExtension Goal Units %d", result.goalUnits);
        
        lt->setSyncGoalUnits(result.goalUnits);

        // for unsynced recordings, calculate the length to add
        result.extensionLength = unitarian->getSingleAutoRecordUnitLength(lt);
    }
    return result;
}


/**
 * While you can always extend, reducing the goal units could
 * retroactively change the meaning of the last sync pulse if it
 * has already been processed in this block.  The conditions where this
 * could happen are very rare but possible:
 *
 *   - block contains a sync unit pulse
 *   - track advances to that pulse and treats it as an extension
 *   - track continues and resumes a script that causes the reduction (e.g. an Undo)
 *   - the last pulse should now be treated as a recording ending pulse
 *     rather than an extension
 *
 * This isn't something we can go back in time for, the script did logically happen
 * AFTER the extension so if you undo at this point the track either needs to ignore
 * it or undo the entire recording.
 *
 * If the reduction attempts to go behind the current recording location there are two schools
 * of though here: return 0 and expect the track to reset the loop.
 * Clamp it at 1 or elapsedUnits and just let the recording finish.
 *
 * Resetting the loop is more consistent with Undo immediately after Record,  but once
 * you've done AutoRecord and passed one elapsed unit, you're more in the zone of using
 * Undo/Redo to adjust the ending location and don't want to reset by accident.
 * Could have a parameter for this, but I'm liking just letting it finish.
 *
 * update: Bernhard noticied this and was expecting Undo to cancel the Record ending
 * first, then Reset second.  That's more in line with how Undo takes pending events away
 * before it takes away the layer.  Ignoring AutoRecord, if you're doing a synchronized
 * recording and accidentally press the second Record to end it, we'll schedule the ending,
 * but now you can't undo what you just did and the recording is forced to complete.
 *
 * AutoRecord is a lot like a synchronized record end.  It's less clear what should happen
 * there, the notion that an AutoRecord becomes an unbounded record if you press Undo once
 * seems weird but it would be consistent with regular syncd record.  But if the alternative
 * is to force it to finish, that seems wrong too, and causing it to completely reset if you
 * back back the unit count down to nothing seems wrong.  So the rules for both sync record
 * and AutoRecord are:
 *
 * Undo will remove one scheduled unit until it counts down to zero or reaches
 * the number of elapsed units.
 *
 * Upon reaching zero/elapsed, the RecordEndEvent is removed and it becomes an unbounded
 * Record.
 *
 * Undo then causes reset.
 *
 * Leave it up to Synchronizer to make that call.  We return zero if we can't go any
 * lower and it can sort out the details.
 *
 */
SyncMaster::RequestResult SyncMaster::requestReduction(int number)
{
    RequestResult result;
    
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {

        int reduction = 1;

        int current = lt->getSyncGoalUnits();
        if (current == 0) {
            // must be in the initial recording before the end frame was set
            // it is unexpected to call this
            current = 1;
        }
        
        int newUnits = current - reduction;
        int unitLength = unitarian->getSingleAutoRecordUnitLength(lt);

        // looking at getSyncElapsedUnits doesn't work for unsynced tracks
        // so do both synced and unsynced the same way by looking at their
        // record location
        int location = lt->getSyncLocation();
        int elapsed = (int)ceil((float)location / (float)unitLength);
        
        if (newUnits < elapsed) {
            // formerly ignired this and just let it finish
            // but that means if you're not using AutoREcord you can never Undo
            // the recording when you're in the first and only unit
            // even for auto record, if you try to go past where you are now it should reset
            //Trace(2, "SyncMaster: Supressing attempt to reduce auto record before elapsed");
            //newUnits = elapsed;
            newUnits = 0;
        }

        result.goalUnits = newUnits;
        lt->setSyncGoalUnits(newUnits);

        // for unsynced recordings, calculate the length to add
        result.extensionLength = unitLength;
    }
    return result;
}

//////////////////////////////////////////////////////////////////////
//
// Block Pulse Injection
//
//////////////////////////////////////////////////////////////////////

/**
 * Called by TimeSlicer to return a sync pulse for this track if it is
 * in the process of doing a synchronized recording.
 */
Pulse* SyncMaster::getBlockPulse(LogicalTrack* track)
{
    Pulse* pulse = nullptr;

    // internal Pulse managed by SM for unit pulses after the recording
    // has started, no longer need to watch sync pulses from the SyncSource
    unitPulse.reset();
    
    if (track->isSyncRecording()) {

        // look for any symc puulses from this track's source
        // these are normally used only for Starting, but MIDI
        // requires some additional beat overagine during record
        Pulse* anyPulse = pulsator->getAnyBlockPulse(track);
        
        if (!track->isSyncRecordStarted()) {
            // still waiting for a pulse from the SyncSource
            if (anyPulse != nullptr) {
                // only slice if this is a relevant pulse
                Pulse* annotated = barTender->annotate(track, anyPulse);
                SyncUnit startUnit = track->getSyncStartUnit();
                if (startUnit == SyncUnitNone) {
                    // should have stored these when we started all this
                    Trace(1, "SyncMaster: Someone forgot to store their units");
                    startUnit = track->getSyncUnit();
                }
                if (isRelevant(annotated, startUnit)) {
                    pulse = annotated;
                }
                else {
                    tracePulse(track, anyPulse);
                }
            }
        }
        else {
            // once recording starts, pulses are generated by watching the
            // frame advance and detecting elapsed record units

            int recordUnitLength = track->getSyncRecordUnitLength();
            int elapsed = track->getSyncElapsedFrames();
            int lastFrameInBlock = elapsed + blockSize - 1;
            
            if (lastFrameInBlock < recordUnitLength) {
                // unit does not transpire in this block
                track->setSyncElapsedFrames(elapsed + blockSize);
            }
            else {
                // Houston, we have a unit
                int pulseOffset = recordUnitLength - elapsed;
                int overage = blockSize - pulseOffset;
                
                // sigh, the Pulse needs a SyncSource to be considered meaningful
                // we don't have SyncSourceInternal and don't need one so reuse
                // SyncSourceTrack
                unitPulse.reset(SyncSourceTrack, 0);
                // don't need to be saving this, it isn't used for internal pulses
                unitPulse.unit = track->getSyncRecordUnit();
                unitPulse.blockFrame = pulseOffset;
                
                // reset the elapsed frame counter for the frames
                // after the sync pulse
                track->setSyncElapsedFrames(overage);

                pulse = &unitPulse;
            }

            // hmm, order of execution issues, this was originally part
            // of handleBlockPulse but that now only gets called when there is a
            // relevant pulse

            // honestly not sure if this works any more, need to test this all over again
            //doContortedMidiShit(track, anyPulse);
        }
    }
    return pulse;
}

bool SyncMaster::isRelevant(Pulse* p, SyncUnit unit)
{
    bool relevant = false;
    
    if (unit == SyncUnitBeat) {
        // anything is a beat
        relevant = true;
    }
    else if (unit == SyncUnitBar) {
        // loops are also bars
        relevant = (p->unit == SyncUnitBar || p->unit == SyncUnitLoop);
    }
    else {
        // only loops will do
        relevant = (p->unit == SyncUnitLoop);
        // formerly had a fallback to accept Bar units if the
        // host didn't support the concept of a Loop, but they
        // all should now and BarTender will flag it
    }
    return relevant;
}

/**
 * Called by TimeSlicer to handle the Pulse that we gave it with
 * getBlockPulse().  The track has been advanced up to this point
 * and we can now mess with it.
 */
void SyncMaster::handleBlockPulse(LogicalTrack* track, Pulse* pulse)
{
    if (!track->isSyncRecordStarted()) {
        // ready to start
        sendSyncEvent(track, pulse, SyncEvent::Start);
        
        track->setSyncRecordStarted(true);
        // the track will now advance the amount remaining in this block
        track->setSyncElapsedFrames(blockSize - pulse->blockFrame);

        int rul = unitarian->getRecordUnitLength(track);
        if (rul == 0) {
            // something went badkly wrong
            Trace(1, "SyncMaster: Unable to calculate record unit length");
            // a little late to be complaining about this, give it something
            rul = 44100;
        }
        
        track->setSyncRecordUnitLength(rul);
    }
    else {
        int goalUnits = track->getSyncGoalUnits();
        int elapsed = track->getSyncElapsedUnits() + 1;
        track->setSyncElapsedUnits(elapsed);

        if (goalUnits == 0) {
            // doing an unbounded record
            //Trace(2, "SM: Unbounded extension");
            sendSyncEvent(track, pulse, SyncEvent::Extend);
        }
        else if (goalUnits == elapsed) {
            // we've reached the end
            sendSyncEvent(track, pulse, SyncEvent::Stop);
        }
        else if (elapsed > goalUnits) {
            // elapsed was not incremented properly, this will be wrong
            // but at least we can stop
            Trace(1, "SyncMaster: Missed goal unit %d %d, stopping late",
                  elapsed, goalUnits);
            sendSyncEvent(track, pulse, SyncEvent::Stop);
        }
    }
}

/**
 * Send one of the sync events to the track.
 */
void SyncMaster::sendSyncEvent(LogicalTrack* t, Pulse* p, SyncEvent::Type type)
{
    SyncEvent event (type);
    
    event.elapsedUnits = t->getSyncElapsedUnits();
    
    traceEvent(t, p, event);
    
    t->syncEvent(&event);
    
    dealWithSyncEvent(t, &event);
}

void SyncMaster::traceEvent(LogicalTrack* t, Pulse* p, SyncEvent& e)
{
    (void)t;
    if (extremeTrace) {
        int head = getSyncPlayHead(t);
        Trace(2, "SM: Event %s block %d offset %d head %d",
              e.getName(), blockCount, p->blockFrame, head);
    }
}

int SyncMaster::getSyncPlayHead(LogicalTrack* t)
{
    int head = 0;
    SyncSource src = getEffectiveSource(t);
    switch (src) {
        case SyncSourceNone: break;
        case SyncSourceMaster: break;
        case SyncSourceTransport: head = transport->getPlayHead(); break;
        case SyncSourceHost: head = hostAnalyzer->getPlayHead(); break;
        case SyncSourceMidi: head = midiAnalyzer->getPlayHead(); break;
        case SyncSourceTrack: {
            if (trackSyncMaster > 0) {
                LogicalTrack* tsm = trackManager->getLogicalTrack(trackSyncMaster);
                head = tsm->getSyncLocation();
            }
        }
            break;
    }
    return head;
}
            
void SyncMaster::tracePulse(LogicalTrack* t, Pulse* p)
{
    (void)t;
    if (extremeTrace) {
        int head = getSyncPlayHead(t);
        Trace(2, "SM: Pulse block %d offset %d head %d",
              blockCount, p->blockFrame, head);
    }
}

/**
 * Called after sending a SyncEvent to a track.
 *
 * For some reason I added the "ended" flag if the track decided to stop
 * recording on this pulse.  But it must also call notifyRecordStopped when
 * it actually processes the ending event which may be delayed for input latency.
 * Wait and do verification until then.  I'm not seeing a purpose for the ending
 * flag other than to verify that it is trying to end.
 *
 * If the track set the error flag, we should abandon the recording.
 */
void SyncMaster::dealWithSyncEvent(class LogicalTrack* lt, SyncEvent* event)
{
    (void)lt;
    if (event->error) {
        Trace(1, "SyncMaster: SyncEvent returned with errors");
    }
    else if (event->ended) {
        // track must do this
        //notifyRecordStopped(lt->getNumber());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Fluxuating Unit Shit
//
//////////////////////////////////////////////////////////////////////

/**
 * True if the sync source for this track has a locked unit.
 * In practice false only for MIDI during the first recording
 * as we let it warm up.
 */
bool SyncMaster::isSourceLocked(LogicalTrack* t)
{
    bool locked = true;
    SyncSource src = t->getSyncSource();
    if (src == SyncSourceMidi)
      locked = midiAnalyzer->isLocked();;
    return locked;
}

/**
 * When synchronizing with MIDI clocks from a "cold start" the unit
 * length may still be smoothing out.  Avoid locking in the unit length
 * for recording until we're close to the end to give it some time to settle down.
 *
 * Could  do this for other sources but MIDI is the most important.
 *
 * The state of the track at this point is:
 *    isSyncRecording
 *    isSyncRecordStarted
 * 
 */
void SyncMaster::doContortedMidiShit(LogicalTrack* track, Pulse* pulse)
{
    SyncSource src = track->getSyncSource();
    int goalUnits = track->getSyncGoalUnits();
    
    if (!track->isSyncFinalized() && src == SyncSourceMidi && goalUnits > 0) {

        // bump the beat count, this is used only for MIDI
        int beat = track->getSyncElapsedBeats() + 1;
        track->setSyncElapsedBeats(beat);
        
        // midi with a goal, watch for the penultimate beat
        int totalBeats = getGoalBeats(track);
        if (beat == (totalBeats - 1)) {
            midiAnalyzer->lock();
            int startingUnit = track->getFundamentalUnitLength();
            int endingUnit = midiAnalyzer->getUnitLength();
            if (startingUnit != endingUnit) {
                Trace(2, "SyncMaster: Adjusting final beat for unit change %d to %d",
                      startingUnit, endingUnit);

                int idealLength = endingUnit * totalBeats;
                int currentLength = track->getSyncLength();
                int unalteredLength = currentLength + endingUnit;
                if (idealLength == unalteredLength) {
                    // it's a miracle! the unit length fluctuated
                    // but we landed in the right place
                    Trace(2, "SyncMaster: No need to adjust final beat, you should be worried");
                }
                else if (currentLength > idealLength) {
                    // you messed something up counting beats
                    Trace(1, "SyncMaster: Ideal length %d less than where we are now %d",
                          idealLength, currentLength);
                }
                else {
                    // this should never be more than a beat, and really a small fraction
                    // unless the user is dicking with the tempo
                    int delta = abs(unalteredLength - idealLength);
                    if (delta > endingUnit)
                      Trace(1, "SyncMaster: Unusually large ending beat adjustment %d",
                            delta);

                    Trace(2, "SyncMaster: Adjusting final beat to end on %d rather than %d",
                          idealLength, unalteredLength);
                        
                    SyncEvent event(SyncEvent::Finalize);
                    event.finalLength = idealLength;
                    track->syncEvent(&event);
                    traceEvent(track, pulse, event);
                    dealWithSyncEvent(track, &event);

                    // from this point forward we won't send SyncEvents to the track
                    // and it will normally end near the next beat
                    // If the user requests an extension during this 1 beat rounding period
                    // then we'll end up back in requestExtension and start dealing
                    // with the goal units with scheduling rather than sync pulses
                    track->setSyncFinalized(true);
                }
            }
        }
    }
}

int SyncMaster::getGoalBeats(LogicalTrack* t)
{
    SyncSource src = t->getSyncSource();
    int unit = t->getSyncUnit();
    int units = t->getSyncGoalUnits();
    int beats = 1;
    
    switch (unit) {
        case SyncUnitBeat: {
            beats = units;
        }
            break;
        case SyncUnitNone:
        case SyncUnitBar: {
            int bpb = barTender->getBeatsPerBar(src);
            beats = units * bpb;
        }
            break;
        case SyncUnitLoop: {
            int bpb = barTender->getBeatsPerBar(src);
            int bpl = barTender->getBarsPerLoop(src);
            beats = (units * bpb) * bpl;
        }
            break;
    }
    if (beats == 0) {
        Trace(1, "SyncMaster: Anomolous goal beats calculation");
        beats = 1;
    }
    return beats;
}

//////////////////////////////////////////////////////////////////////
//
// Track Notifications
//
// These are expected to be called when a track enters various states.
// This may have side effects if this track is also the TrackSyncMaster
// or TransportMaster.
//
//////////////////////////////////////////////////////////////////////

/**
 * This is called when a track begins recording.
 * If this is the TransportMaster, Synchronizer in the past would do a "full stop"
 * to send a STOP event and stop sending MIDI clocks.
 */
void SyncMaster::notifyRecordStarted(int number)
{
    // continue calling MidiRealizer but this needs to be under the control of the Transport
    if (number == transport->getMaster()) {
        transport->stop();
        // unlike notifyTrackReset, the master connection remains
    }
}

/**
 * This is called when a recording has offically ended.
 * It may have been synced or not.  It will be after inputLatency
 * and ready to start recording.
 *
 * This also makes the track available for mastership.
 *
 * This is the best place to do final verification on obeyance of the
 * sync unit length.
 */
void SyncMaster::notifyRecordStopped(int number)
{
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {

        if (lt->isSyncRecording()) {
            
            // this stops sending pulses to the track
            lt->setSyncRecording(false);

            // final verification on sync unit obeyance
            unitarian->verifySyncLength(lt);
        }
        // else it's a free record
         
        notifyTrackAvailable(number);
        lt->resetSyncState();

        // also inform any followers
        // which MIDI tracks use to adjust their size and start playing
        // if this is from an audio track, due to input latency these will be
        // a little late so we might want to adjust that so they go ahread
        // a little, the issue is very similar to to pre-playing the record layer,
        // but since MidiTrack just follows the record frame we can't do that
        // reliably yet
        // !! fuck, the Notifier interface is shit and needs the core Track
        // to be passed in, have to continue duplicating this in Synchronizer
        // for audio and LooperScheduler for MIDI until this is sorted out...
        //Notifier* n = kernel->getNotifier();
        //n->notify(lt, NotificationRecordEnd);
    }
}

/**
 * Called when a track has finished recording and may serve as a sync master.
 *
 * If there is already a sync master, it is not changed, though we should
 * allow a special sync mode, maybe SyncSourceMasterForce or some other
 * parameter that overrides it.
 *
 * Also worth considering if we need an option for tracks to not become the
 * track sync master if they don't want to.
 */
void SyncMaster::notifyTrackAvailable(int number)
{
    // verify the number is in range and can be a leader
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {
        // anything can become the track sync master
        if (trackSyncMaster == 0)
          trackSyncMaster = number;

        SyncSource src = lt->getSyncSource();
        
        if (src == SyncSourceMaster) {
            // this one wants to be special
            int currentMaster = transport->getMaster();
            if (currentMaster == 0) {

                connectTransport(number);
            }
            else if (currentMaster == number) {
                // this track was already the transport master
                // and we're being told it was re-recorded
                // this can happen if you switch to an empty loop then start
                // a new recording in that loop, 
                // notifyTrackRecord will stop the clocks but it won't take
                // away mastership, it could but I think the intent would be
                // to have this track continue as the master rather than assign
                // another one at random
                connectTransport(number);
            }
            else {
                // this can't be the sync master, it will revert
                // to either SourceLeader or SourceTransport
                // can make that decision later
            }
        }
    }
}

/**
 * Called when a track is reset, if this was one of the sync masters
 * Synchronizer would try to auto-assign another one.
 *
 * This unlocks the follower automatically.
 *
 * If this was the TrackSyncMaster, this is where the old Synchronizer would choose
 * a new one.  Unclear how I want this to work.  We would have to pick one at random
 * which is hard to predict.   Wait until the next new recording which calls
 * notifyTrackAvailable or make the user do it manually.
 *
 * If this was the TransportMaster, old Synchronizer would send a MIDI Stop command.
 *
 * NOTE: This is now being called when the track switches to a loop that is empty.
 * OG Mobius would not stop clocks when that happened.  That seems inconsistent.
 * The loop is "silent" so it is similar to entering Mute mode but we don't have
 * a callback for notifyLoopEmpty or notifyLoopSwitch and then checking for empty.
 * Revisit based on user requests.
 */
void SyncMaster::notifyTrackReset(int number)
{
    if (number == trackSyncMaster) {
        // Synchronizer used to choose a different one automatically
        // It looks like of confusing to see this still show as TrackSyncMaster in the UI
        // so reset it, but don't pick a new one
        setTrackSyncMaster(0);
        if (trackMasterReset == TrackMasterMove)
          assignTrackSyncMaster(number);
    }

    if (number == transport->getMaster()) {
        // Synchronizer would send MIDI Stop at this point
        // it had a method fullStop that I think both sent the STOP event
        // and stop generating clocks
        transport->stop();

        transport->disconnect();
    }

    // it can no longer be recording
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr)
      lt->resetSyncState();
}

/**
 * Here when the TSM has been reset and TrackMasterReset=Move
 * Look for another track to automatically become the TSM.
 *
 * The expectataion of many is that these go in a particular direction
 * relative to the track that was just reset.  Until it is requested, look
 * to the right.
 */
void SyncMaster::assignTrackSyncMaster(int former)
{
    int first = 0;
    int priority = 0;

    int max = trackManager->getTrackCount();
    
    for (int i = former + 1 ; i <= max ; i++) {
        LogicalTrack* lt = trackManager->getLogicalTrack(i);
        if (lt != nullptr && lt->getSyncLength() > 0) {
            TrackMasterSelect tms = (TrackMasterSelect)lt->getParameterOrdinal(ParamTrackMasterSelect);
            if (tms == TrackMasterPriority) {
                priority = i;
                break;
            }
            else if (tms == TrackMasterAccept) {
                if (first == 0)
                  first = i;
            }
        }
    }
    
    if (priority == 0) {
        // keep looking from the front
        for (int i = 1 ; i < former ; i++) {
            LogicalTrack* lt = trackManager->getLogicalTrack(i);
            if (lt != nullptr && lt->getSyncLength() > 0) {
                TrackMasterSelect tms = (TrackMasterSelect)lt->getParameterOrdinal(ParamTrackMasterSelect);
                if (tms == TrackMasterPriority) {
                    priority = i;
                    break;
                }
                else if (tms == TrackMasterAccept) {
                    if (first == 0)
                      first = i;
                }
            }
        }
    }
    
    if (priority != 0)
      setTrackSyncMaster(priority);
    else if (first != 0)
      setTrackSyncMaster(first);
}

/**
 * Called when a track has restructured in some way.  Mostly we care about
 * the length of the track loop, but might also be sensitive to cycle counts.
 *
 * This is very similar to notifyTrackAvailable in that when a track moves
 * from Reset to Play after various actions like Switch, Load, or Undo, it could
 * also become a sync master.  Not doing that yet.
 *
 * If the track IS ALREADY the TransportMaster, then the transport must be
 * reconfigured and the tempo may change.
 */
void SyncMaster::notifyTrackRestructure(int number)
{
    if (number == transport->getMaster()) {
        // we don't need to distinguish between restructuring
        // and establishing a connection right now
        connectTransport(number);
    }
}

/**
 * Called when a track Restarts.
 *
 * A Restart means that the track abruptly went to start point through
 * a user action rather than simply playing normally to the end and looping.
 * There are many reasons for this including functions: Start, Realign, StartPoint,
 * UnroundedMultiply, and Trim.  Also LoopSwitch with switchLocation=Start and
 * Unmute with muteMode=Start.
 *
 * When this happens OG Mobius would send MIDI Start if this track was the OutSyncMaster
 * (now the TransportMaster).  The intent here to realign an external MIDI
 * sequencer so that it would play from the start along with the track.
 *
 * This was controlled with another "manual strt" option.  When this was on, it would
 * wait until the user explicitly used the functions MidiStart or MuteMidiStart
 * to schedule when the MIDI Start would be sent.  In those cases the track must call
 * back to notifyMidiStart.
 *
 * todo: Now that the user can interact with the transport using the UI, there are other
 * ways to request a MIDI start.  These are not the same as using the old MidiStart function
 * and do not currrently have any synchronization options.  I think I'd like to keep it that
 * way, if the user presses the Start button, it should just start.  If this suspends waiting
 * for something in the master track, then we have yet another form of event scheduling
 * and "synchronize mode" that would have to be visualized in the UI.  That's what the
 * MidiStart function is for, though a better name for that might be SyncMidiStart or
 * RequestMidiStart.
 *
 * While transportManualStart is technically a Transport parameter, we test it out here
 * because SyncMaster has the context necessary to know whether this is an auto-start
 * or an explicit start.
 */
void SyncMaster::notifyTrackRestart(int number)
{
    if (number == transport->getMaster()) {
        if (!transport->isManualStart())
          transport->start();
    }
}

/**
 * Callback for the MidiStart and MuteMidiStart functions.  The user explicitly
 * asked for a Start so we don't test ManualStart.
 *
 * OG Mobius had some thinking around "checkNear" which tried to determine if the
 * external MIDI loop was already near the start point and if so avoided sending
 * a redundant MIDI Start which could cause "flamming" of drum notes that had already
 * been recently played.  This might sound better to the user, but it would put the
 * external pattern slightly out of alignment with where the master loop is.
 * I decided not to carry that forward but it could be useful.  If you do decide
 * to bring that back it should apply to notifyTrackRestart as well.
 */
void SyncMaster::notifyMidiStart(int number)
{
    // does this have to be the TransportMaster or can it be sent from anywhere?
    (void)number;
    transport->start();
}

/**
 * Called when the track has entered a state of Pause.
 * This can happen with the Pause or GlobalPause functions, or the Stop
 * function which both pauses and rewinds.
 *
 * It also happens indirectly when a project is loaded and puts all tracks
 * in Pause mode.
 *
 * OG Synchronzier had MuteSyncMode that would control whether to stop clocks
 * whenever the loop became silent.  In the EDP this was only for Mute mode
 * but I extended it to apply to Pause as well.  Since SyncMaster is handling
 * ManualStart it also needs transportMuteStop to control what happens when
 * in Mute/Pause modes.  For now, assuming we stop.
 */
void SyncMaster::notifyTrackPause(int number)
{
    if (number == transport->getMaster()) {
        
        // todo: transportMuteStop parmater to disable this
        transport->stop();
    }
}

/**
 * Called when the track exists Pause.
 *
 * OG Synchronizer didn't do anything special here, but this is the place
 * where it should be trying to send SongPositionPointer.
 *
 * The complication is that MIDI Continue requires a song position pointer,
 * and these are coarser grained than an audio stream frame location
 * the MIDI Continue would need to be delayed until the Transport actually
 * reaches that song position.
 */
void SyncMaster::notifyTrackResume(int number)
{
    if (number == transport->getMaster()) {
        // !! probably wrong, need to address song position
        transport->start();
    }
}

/**
 * Called when a track enters Mute mode.
 *
 * Old Synchronizer had MuteSyncMode options to send a MIDI Stop event when this happened,
 * and then other options about what happened when the track unmuted.
 * Those should be moved to Transport parameters.  As it stands now, unmute
 * options are internal to Mobius and it will call back to Start or Resume.
 */
void SyncMaster::notifyTrackMute(int number)
{
    // punt for now
    (void)number;
}

/**
 * Called when a track jumps to a new location rather than advancing normally.
 * This could be used to send MIDI song position pointers which is hard.
 *
 * Making this different than notifyTrackStart which is overloaded with other options.
 */
void SyncMaster::notifyTrackMove(int number)
{
    (void)number;
}

/**
 * Called when a track changes playback rate.
 *
 * In theory this could adjust the tempo of the Transport and MIDI clocks.
 * Don't remember what old Synchronizer did, it probably tried to change the tempo
 * poorly.
 */
void SyncMaster::notifyTrackSpeed(int number)
{
    (void)number;
}

/**
 * This is called when OG Mobius evaluates the MidiStop function event.
 *
 * Comments say this was "usually scheduled for the start point" but I don't
 * see that happening.  It would either happen immediately or stacked on a loop
 * switch, and probably after RecordStopEvent latency.
 *
 * A better name for this would be SyncMidiStop to distinguish it from
 * TransportStop which is always immediate.
 *
 * !! The old functions with "Midi" in their names need to be revisited.
 */
void SyncMaster::notifyMidiStop(int number)
{
    (void)number;
}

//////////////////////////////////////////////////////////////////////
//
// Old Synchronizer Code
//
// This is here for reference only, showing what it used to do and we
// need to replicate in some way.
//
//////////////////////////////////////////////////////////////////////

/**
 * Copied from the old Synchroonzer for reference
 *
 * It is assumed we've already checked for the track being the TransportMaster
 * and something happened that requires a MIDI start.
 *
 * The checkManual flag says to look at an old config option to suppress
 * automatic starts.
 *
 * The checkNear flag is stubbed and used the SyncTracker which no longer exists.
 * I think the intent was to suppress starts when we know that we're already pretty k
 * close to the start point, I guess to prevent flamming if you're driving a drum machine.
 */
#if 0
void SyncMaster::sendStart(bool checkManual, bool checkNear)
{
	bool doStart = true;

    // probably still want this, but get it from the Session
	if (checkManual) {
        //Setup* setup = mMobius->getActiveSetup();
        //doStart = !(setup->isManualStart());
	}

	if (doStart) {
		// To avoid a flam, detect if we're already at the external
		// start point so we don't need to send a START.
        // !! We could be a long way from the pulse, should we be
        // checking frame advance as well?
        
        bool nearStart = false;
        if (checkNear) {
            /*
            int pulse = mOutTracker->getPulse();
            if (pulse == 0 || pulse == mOutTracker->getLoopPulses())
              nearStart = true;
            */
        }

        if (nearStart && isMidiOutStarted()) {
			// The unit tests want to verify that we at least tried
			// to send a start event.  If we suppressed one because we're
			// already there, still increment the start count.
            Trace(2, "SyncMaster: Suppressing MIDI Start since we're near\n");
			midiRealizer->incStarts();
        }
        else {
            Trace(2, "SyncMaster: Sending MIDI Start\n");
            // mTransport->start(l);
            midiRealizer->start();
		}
	}
}

/**
 * Called when a track pauses.
 * If this is the TransportMaster we have sent MIDI Stop
 * !! this needs to be moved inside Transport
 */
void SyncMaster::notifyTrackStop(int number)
{
    if (transport->getMaster() == number) {
        muteMidiStop();
    }
}

/**
 * After entering Mute or Pause modes, decide whether to send
 * MIDI transport commands and stop clocks.  This is controlled
 * by an obscure option MuteSyncMode.  This is for dumb devices
 * that don't understand STOP/START/CONTINUE messages.
 *
 * todo: decide if we still need this
 */
void SyncMaster::muteMidiStop()
{

    //Setup* setup = mMobius->getActiveSetup();
    //MuteSyncMode mode = setup->getMuteSyncMode();
    MuteSyncMode mode = MUTE_SYNC_TRANSPORT;

    bool doTransport = (mode == MUTE_SYNC_TRANSPORT ||
                        mode == MUTE_SYNC_TRANSPORT_CLOCKS);
    
    bool doClocks = (mode == MUTE_SYNC_CLOCKS ||
                     mode == MUTE_SYNC_TRANSPORT_CLOCKS);

    // mTransport->stop(l, transport, clocks);
    // what "transport" means here is sending MIDI start/stop messages
    midiRealizer->stopSelective(doTransport, doClocks);
}

/**
 * Called when a track resumes from Pause
 */
void SyncMaster::notifyTrackResume(int number)
{
    // !! this needs to be moved inside Transport
    if (transport->getMaster() == number) {
        //Setup* setup = mMobius->getActiveSetup();
        //MuteSyncMode mode = setup->getMuteSyncMode();
        MuteSyncMode mode = MUTE_SYNC_TRANSPORT;
        
        if (mode == MUTE_SYNC_TRANSPORT ||
            mode == MUTE_SYNC_TRANSPORT_CLOCKS) {
            // we sent MS_STOP, now send MS_CONTINUE
            //mTransport->midiContinue(l);
            midiRealizer->midiContinue();
        }
        else  {
            // we just stopped sending clocks, resume them
            // mTransport->startClocks(l);
            midiRealizer->startClocks();
        }
    }
}

/**
 * Called by Loop when it enters Mute mode.
 * 
 * When MuteMode=Start the EDP would stop clocks then restart them
 * when we restart comming out of mute.  Feels like another random
 * EDPism we don't necessarily want, should provide an option to keep
 * clocks going and restart later.
 */
void SyncMaster::notifyTrackMute(int number)
{
	if (transport->getMaster() == number) {
		//Preset* p = l->getPreset();
        //MuteMode mode = p->getMuteMode();
        ParameterMuteMode mode = MUTE_START;
        
		if (mode == MUTE_START) 
          muteMidiStop();
	}
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Internal Component Services
//
//////////////////////////////////////////////////////////////////////

// who uses this?
SyncUnit SyncMaster::getSyncUnit(int id)
{
    SyncUnit unit = SyncUnitBeat;
    LogicalTrack* t = trackManager->getLogicalTrack(id);
    if (t != nullptr) {
        unit = t->getSyncUnit();
    }
    return unit;
}

/**
 * Needed by BarTender, and eventually TimeSlicer if you move it under here.
 */
TrackManager* SyncMaster::getTrackManager()
{
    return kernel->getTrackManager();
}

/**
 * Needed by MidiAnalyzer so it can pull things from the Session
 */
SymbolTable* SyncMaster::getSymbols()
{
    return kernel->getContainer()->getSymbols();
}

int SyncMaster::getRecordThreshold()
{
    return recordThreshold;
}

/**
 * MidiRealizer does this for MIDI device issues.
 * This needs to end up in Supervisor::addAlert and handled in the
 * UI thread.
 */
void SyncMaster::sendAlert(juce::String msg)
{
    kernel->sendAlert(msg);
}

/**
 * Must be called during track advance by anything that can lead.  Will
 * be ignored unless something is following it.
 */
void SyncMaster::addLeaderPulse(int leader, SyncUnit unit, int frameOffset)
{
    pulsator->addLeaderPulse(leader, unit, frameOffset);
}

//////////////////////////////////////////////////////////////////////
//
// Old core Variable Support
//
// These are old and should only be used for some core script Variables.
// Weed these out in time.
// I put "var" in front of them to make it clear what they're intended for
// the rest of the system shouldn't be using these.
//
//////////////////////////////////////////////////////////////////////

bool SyncMaster::varIsMidiOutSending()
{
    return midiRealizer->isSending();
}
bool SyncMaster::varIsMidiOutStarted()
{
    return midiRealizer->isStarted();
}

int SyncMaster::varGetMidiInRawBeat()
{
    return midiAnalyzer->getElapsedBeats();
}
bool SyncMaster::varIsMidiInReceiving()
{
    return midiAnalyzer->isReceiving();
}
bool SyncMaster::varIsMidiInStarted()
{
    return midiAnalyzer->isRunning();
}

//
// Interfaces that take just a SyncSource are obsolete and
// only used by old core/Variable and core/Synchronizer code
// These will be phased out
//

int SyncMaster::varGetBeat(SyncSource src)
{
    (void)src;
    return barTender->getBeat(0);
}

int SyncMaster::varGetBar(SyncSource src)
{
    (void)src;
    return barTender->getBar(0);
}

int SyncMaster::varGetBeatsPerBar(SyncSource src)
{
    (void)src;
    return barTender->getBeatsPerBar(0);
}

float SyncMaster::varGetTempo(SyncSource src)
{
    float tempo = 0.0f;
    switch (src) {
        
        case SyncSourceHost: {
            tempo = (float)hostAnalyzer->getTempo();
        }
            break;

        case SyncSourceMidi: {
            // Pulsator also tracks this but we can get it directly from the Analyzer
            tempo = midiAnalyzer->getTempo();
        }
            break;

        case SyncSourceMaster:
        case SyncSourceTransport: {
            // these are now the same
            tempo = transport->getTempo();
        }
            break;

        default: break;
    }
    return tempo;
}

//////////////////////////////////////////////////////////////////////
//
// Drift Testing
//
//////////////////////////////////////////////////////////////////////

/**
 * At the end of each block, look at the various sync sources that
 * can have drift and if they have reached a loop or bar point in this
 * block, trace the current drift.
 *
 * It doesn't really matter when we trace drift, it just needs to come
 * out at interesting moments and not too fast.o
 */
void SyncMaster::checkDrifts()
{
    SyncAnalyzerResult* res = hostAnalyzer->getResult();
    if (res->beatDetected) {
        // every 4 beats is good enough for now
        int beat = hostAnalyzer->getElapsedBeats();
        if ((beat % 4) == 0) {
            int drift = hostAnalyzer->getDrift();
            // wobbling between -1 and 0 is common
            if (abs(drift) > 1)
              Trace(2, "SyncMaster: Host drift %d", drift);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
