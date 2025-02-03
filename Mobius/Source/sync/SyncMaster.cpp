
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/Symbol.h"
#include "../model/Session.h"
#include "../model/UIAction.h"
#include "../model/Query.h"
#include "../model/SystemState.h"
#include "../model/SyncState.h"
#include "../model/TrackState.h"
#include "../model/PriorityState.h"

// for some of the old sync related modes
#include "../model/ParameterConstants.h"

#include "../mobius/MobiusKernel.h"
#include "../mobius/track/TrackManager.h"
#include "../mobius/track/LogicalTrack.h"
#include "../mobius/track/TrackProperties.h"

#include "Pulsator.h"
#include "Pulse.h"
#include "MidiRealizer.h"
#include "MidiAnalyzer.h"
#include "HostAnalyzer.h"
#include "Transport.h"

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
    pulsator.reset(new Pulsator(this, trackManager, barTender.get()));

    // reach out and touch the face of god
    hostAnalyzer->initialize(container->getAudioProcessor());

    MidiManager* mm = container->getMidiManager();
    midiRealizer->initialize(this, mm);
    midiAnalyzer->initialize(this, mm);

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

    // cached parameters
    manualStart = sessionHelper.getBool(s, ParamTransportManualStart);
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
}

/**
 * Only for TimeSlicer, don't need a list yet.
 */
void SyncMaster::addListener(Listener* l)
{
    listener = l;
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
void SyncMaster::notifyTrackRecord(int number)
{
    // continue calling MidiRealizer but this needs to be under the control of the Transport
    if (number == transport->getMaster()) {
        transport->stop();
        // unlike notifyTrackReset, the master connection remains
    }
}

/**
 * This is called when a track is preparing to end a recording.
 * If the track is not following another track, the ending may need to be adjusted
 * so that it have a final length that is even and results in the calculation
 * of a stable unit length.
 *
 * The adjustment if any is returned and the final loop length should be shorter
 * or longer at the time notifyTrackAvailable is called.
 *
 * This requires all the same calculations that Transport::connect does allowing
 * it to return the ideal length.  Needs work...
 */
int SyncMaster::notifyTrackRecordEnding(int number)
{
    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt != nullptr) {

        // for MIDI mostly, but potentially others, remember the unit length
        // used to record this track
        SyncSource src = lt->getSyncSourceNow();
        if (src == SyncSourceMidi) {
            int unitLength = midiAnalyzer->getUnitLength();
            if (unitLength == 0) {
                // should have locked this before or immediately
                // after recording began
                Trace(1, "SyncMaster: Failed to lock unit length during recording");
            }
            else {
                lt->setUnitLength(unitLength);
            }
        }
    }
    
    return 0;
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

        SyncSource src = lt->getSyncSourceNow();
        
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
        trackSyncMaster = 0;
    }

    if (number == transport->getMaster()) {
        // Synchronizer would send MIDI Stop at this point
        // it had a method fullStop that I think both sent the STOP event
        // and stop generating clocks
        transport->stop();

        transport->disconnect();
    }
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
        if (!manualStart) 
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
        // !! probably wrong
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
// Masters
//
//////////////////////////////////////////////////////////////////////

// !! Need to think more about the concepts of Connect and Disconnect
// for the TransportMaster

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
        if (listener != nullptr)
          listener->syncFollowerChanges();
    }
}

int SyncMaster::getTrackSyncMaster()
{
    return trackSyncMaster;
}

/**
 * Action handler for FuncSyncMasterTrack
 * Formerly implemented as a Mobius core function.
 * This took no arguments and made the active track the master.
 *
 * Now this makes the focused track the master which may include MIDI tracks.
 * To allow more control, the action may have an argument with a track number.
 * todo: This needs to be expanded to accept any form of trackk identifier.
 */
void SyncMaster::setTrackSyncMaster(UIAction* a)
{
    int number = a->value;
    if (number == 0) {
        // todo: not liking how track focus is passed around and where it lives
        number = container->getFocusedTrackIndex() + 1;
    }

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt == nullptr)
      Trace(1, "SyncMaster: Invalid track id in SyncMasterTrack action");
    else
      setTrackSyncMaster(number);
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

int SyncMaster::getTransportMaster()
{
    return transport->getMaster();
}

/**
 * Action handler for FuncSyncMasterMidi
 * Formerly implemented as a Mobius core function.
 * This took no arguments and made the active track the "MIDI Master".
 *
 * This is now the equivalent of setting the TransportMaster.
 * The name "SyncMasterMidi" is kept for backward compatibility but it should
 * be made an alias of TransportMaster.
 *
 * Like SyncMasterTrack this now makes the focused track the master which may
 * include MIDI tracks.
 */
void SyncMaster::setTransportMaster(UIAction* a)
{
    int number = a->value;
    if (number == 0) {
        // todo: not liking how track focus is passed around and where it lives
        number = container->getFocusedTrackIndex() + 1;
    }

    LogicalTrack* lt = trackManager->getLogicalTrack(number);
    if (lt == nullptr)
      Trace(1, "SyncMaster: Invalid track id in TransportMaster action");
    else {
        setTransportMaster(number);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * This must be called very early in the kernel block processing phase.
 * It initializes the subcomponents for the call to advance() which
 * happens after various things in the kernel, such as action handling.
 *
 * The decision whether to put something here or in advance() is vague.
 * Basically this should only set things up for advance without thinking
 * too hard, and advance does the interestingn work.
 */
void SyncMaster::beginAudioBlock(MobiusAudioStream* stream)
{
    // monitor changes to the sample rate once the audio device is pumping
    // and adjust internal calculations
    // this should come through MobiusAudioStream, that's where it lives
    int newSampleRate = stream->getSampleRate();
    if (newSampleRate != sampleRate)
      refreshSampleRate(newSampleRate);

    // pull in the host information before we get very far
    hostAnalyzer->analyze(stream->getInterruptFrames());
}

void SyncMaster::refreshSampleRate(int rate)
{
    sampleRate = rate;
    
    hostAnalyzer->setSampleRate(rate);
    transport->setSampleRate(rate);
    midiRealizer->setSampleRate(rate);
    midiAnalyzer->setSampleRate(rate);
}

void SyncMaster::advance(MobiusAudioStream* stream)
{
    int frames = stream->getInterruptFrames();

    // once we start receiving audio blocks, it is okay to start converting
    // MIDI events into MidiSyncMessages, if you allow event queueing before
    // blocks come in, the queue can overflow
    // todo: while not normally a problem, if you disconnect the audio stream
    // after initialization, then the queue will get stuck and overflow, the maintenance
    // thread could monitor for a suspension of audio blocks and disable the queue
    enableEventQueue();

    // host was advanced in beginAudioBlock
    
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

    // Detect whether MIDI clocks have stopped comming in
    //
    // Supervisor formerly called this on the maintenance thread
    // interval, since we don't get a performMaintenance ping down here
    // we can check it on every block, the granularity doesn't really matter
    // since it is based off millisecond time advance
    midiAnalyzer->checkClocks();

    // temporary diagnostics
    checkDrifts();
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

/**
 * The only action receiver right now is Transport.
 * There isn't anything about host/midi sync that is under the user's
 * control.
 */
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
            
        default:
            // Transport is the only thing under us that takes actions
            handled = transport->doAction(a);
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
    bool handled = false;

    // todo: the masters, host and midi settings

    handled = transport->doQuery(q);
    
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
    // !! need to seriously rethink kthe utility of track-specific BPB and BPL
    // overrides, why can't this just be global?  it only really matters
    // for the initial recording, then it's just for display
    
    // Analyzer fills everything except normalized beats
    midiAnalyzer->refreshState(state);
    state->midiBeat = barTender->getBeat(SyncSourceMidi);
    state->midiBar = barTender->getBar(SyncSourceMidi);
    state->midiLoop = barTender->getLoop(SyncSourceMidi);
    state->midiBeatsPerBar = barTender->getBeatsPerBar(SyncSourceMidi);
    state->midiBarsPerLoop = barTender->getBeatsPerBar(SyncSourceMidi);
    
    // the host doesn't have a UI element since you're usually just watching the
    // host UI, but if you have overrides it should
    // same issues about global vs. track-specific time signatures as MIDI sync
    state->hostStarted = hostAnalyzer->isRunning();
    state->hostTempo = hostAnalyzer->getTempo();

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
            tstate->syncSource = lt->getSyncSourceNow();
            tstate->syncUnit = lt->getSyncUnitNow();

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
}

//////////////////////////////////////////////////////////////////////
//
// Internal Component Services
//
//////////////////////////////////////////////////////////////////////

/**
 * This is in about a dozen places now.  Formerly tried to force this
 * through MobiusContainer but I'm tired of hiding Juce.
 *
 * Within the SyncMaster subcomponents, it would be good to have a stable
 * millisecond number that is captured at the start of each audio block rather
 * than going back to Juce each time, which might come back with a different number.
 */
int SyncMaster::getMilliseconds()
{
    return juce::Time::getMillisecondCounter();
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
 * Return the relevant block pulse for a follower.
 * e.g. don't return Beat if the follower wants Bar
 * Used by TimeSlicer to slice the block around pulses.
 */
Pulse* SyncMaster::getBlockPulse(int trackNumber)
{
    return pulsator->getRelevantBlockPulse(trackNumber);
}

/**
 * Get the effective sync source for a track.
 * The complication here is around SourceMaster which is only allowed
 * if there is no other sync master.
 */
SyncSource SyncMaster::getEffectiveSource(int id)
{
    SyncSource source = SyncSourceNone;
    
    LogicalTrack* t = trackManager->getLogicalTrack(id);
    if (t != nullptr) {
        source = t->getSyncSourceNow();
        if (source == SyncSourceMaster) {
            int transportMaster = transport->getMaster();
            if (transportMaster > 0 && transportMaster != id) {
                // there is already a transport master, this track
                // reverts to following the transport
                source = SyncSourceTransport;
            }
        }
    }
    return source;
}

SyncUnit SyncMaster::getSyncUnit(int id)
{
    SyncUnit unit = SyncUnitBeat;
    LogicalTrack* t = trackManager->getLogicalTrack(id);
    if (t != nullptr) {
        unit = t->getSyncUnitNow();
    }
    return unit;
}

/**
 * Must be called during track advance by anything that can lead.  Will
 * be ignored unless something is following it.
 */
void SyncMaster::addLeaderPulse(int leader, SyncUnit unit, int frameOffset)
{
    pulsator->addLeaderPulse(leader, unit, frameOffset);
}

/**
 * A follower is "active" it it uses this sync source and it is not empty (in reset).
 * This is called only by MidiAnalyzer ATM to know whether it is safe to make continuous
 * adjustments to the locked unit length or whether it needs to retain the current unit
 * length and do drift notifications.
 *
 * Once fully recorded, a follower is only active if it was recorded with the same unit
 * length that is active now.  This allows the following to be broken after the user deliberately
 * changes the device tempo, forcing a unit recalculation which is then used for new recordings.
 */
int SyncMaster::getActiveFollowers(SyncSource src, int unitLength)
{
    int followers = 0;
    
    for (int i = 0 ; i < trackManager->getTrackCount() ; i++) {
        LogicalTrack* lt = trackManager->getLogicalTrack(i+1);
        if (lt->getSyncSourceNow() == src) {
            // todo: still some lingering issues if the track has multiple loops
            // and they were recorded with different unit lenghts, that would be unusual
            // but is possible
            if (lt->getUnitLength() == unitLength)
              followers++;
        }
    }
    return followers;
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
// Bar Lengths
//
//////////////////////////////////////////////////////////////////////

/**
 * Old code scraped from Synchronizer, we need to do something similar here.
 *
 * This is part of AutoRecord, see notes
 */
int SyncMaster::getBarFrames(SyncSource src)
{
    (void)src;
    
#if 0        
    if (src == SYNC_HOST) {
        if (mHostTracker->isLocked()) {
            // we've already locked the beat length, normally this
            // will have been rounded before locking so we won't have a fraction
            unit->frames = mHostTracker->getPulseFrames();
        }
        else {
            // NOTE: Should we use what the host tells us or what we measured
            // in tye SyncTracker?  Assuming we should follow the host.
            traceTempo(l, "Host", mHostTempo);
            unit->frames = getFramesPerBeat(mHostTempo);
        }
    }
#endif                               

#if 0
       if (mMidiTracker->isLocked()) {
            // We've already locked the pulse length, this may have a fraction
            // but normally we will round it up so that when multiplied by 24
            // the resulting beat width is integral
            float pulseFrames = mMidiTracker->getPulseFrames();
            unit->frames = pulseFrames * (float)24;
        }
        else {
            // Two tempos to choose from, the average tempo and
            // a smoothed tempo rounded down to a 1/10th.
            // We have an internal parameter to select the mode, figure
            // out the best one and stick with it!

            float tempo = mSyncMaster->getMidiInTempo();
            traceTempo(l, "MIDI", tempo);

            int smooth = mSyncMaster->getMidiInSmoothTempo();
            float fsmooth = (float)smooth / 10.0f;
            traceTempo(l, "MIDI smooth", fsmooth);

            float frames = getFramesPerBeat(tempo);
            float sframes = getFramesPerBeat(fsmooth);

            Trace(l, 2, "Sync: getRecordUnit average frames %ld smooth frames %ld\n",
                  (long)frames, (long)sframes);
        
            if (mMidiRecordMode == MIDI_TEMPO_AVERAGE)
              unit->frames = frames;
            else
              unit->frames = sframes;
        }
#endif

       return 4;

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
            Trace(2, "SyncMaster: Host drift %d", drift);
        }
    }
}


//////////////////////////////////////////////////////////////////////
//
// Auto Record
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the number of frames in one AutoRecord "unit".
 * This is defined by the base(beat) unit length in the track's
 * sync source combined with the autoRecordUnit and autoRecordUnits
 * parameters.
 *
 * This takes the place of these older parameters:
 *
 *       autoRecordTempo
 *       autoRecordBars
 *
 * It would be nice to simplify this by saying that when you have a SyncSource
 * the SyncUnit you to synchronize the start of the recording will be the
 * one used for AutoRecord.  For example if syncUnit=bar, then recording will
 * wait for a host bar and if you want to AutoRecord, you will also want that
 * in units of a host bar.
 *
 * They COULD however be independent, but I don't think many will want that.
 * The possible exception is when sync=None.  In that case there syncUnit
 * is usually undefined, but since this will fall back to the Transport tempo
 * to calculate the unit length, we might as well also use whatever syncUnit
 * happens to be for AutoRecord too.  The alternative would be a bunch of flexible
 * but confusing new parameters.
 *
 */
int SyncMaster::getAutoRecordUnitLength(int number)
{
    int frames = 0;
    
    LogicalTrack* track = trackManager->getLogicalTrack(number);
    if (track != nullptr) {
        
        frames = barTender->getUnitLength(track);

        // if the syncUnit is bar or loop then the beat unit length
        // is multipled by whatever the beatsPerBar for that source is
        SyncUnit unit = track->getSyncUnitNow();
        if (unit == SyncUnitBar || unit == SyncUnitLoop) {
            int barMultiplier = barTender->getBeatsPerBar(track);
            frames *= barMultiplier;
        }

        // if the syncUnit is Loop, then one more multiple
        if (unit == SyncUnitLoop) {
            int loopMultiplier = barTender->getBarsPerLoop(track);
            frames *= loopMultiplier;
        }
    }
    return frames;
}

/**
 * Return the number of units as returned by getAutoRecordUnitLength
 * are to be included in an AutoRecord.
 *
 * This is defined by another parameter.  The two are normally multiplied
 * to gether to get the total length with the recordUnits number becomming
 * the number of cycles in the loop.
 */
int SyncMaster::getAutoRecordUnits(int number)
{
    // this one is not senstiive to the syncSource
    (void)number;
    
    int autoRecordUnits = sessionHelper.getInt(ParamAutoRecordBars);
    if (autoRecordUnits <= 0)
      autoRecordUnits = 1;
    return autoRecordUnits;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
