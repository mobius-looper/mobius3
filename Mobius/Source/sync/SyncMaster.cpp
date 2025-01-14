
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/Symbol.h"
#include "../model/Session.h"
#include "../model/UIAction.h"
#include "../model/Query.h"
#include "../model/PriorityState.h"

// for some of the old sync related modes
#include "../model/ParameterConstants.h"

#include "../mobius/MobiusKernel.h"
#include "../mobius/track/TrackManager.h"
#include "../mobius/track/TrackProperties.h"

#include "Pulsator.h"
#include "Pulse.h"
#include "MidiRealizer.h"
#include "MidiAnalyzer.h"
#include "HostAnalyzer.h"
#include "Transport.h"
#include "SyncMasterState.h"

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
void SyncMaster::initialize(MobiusKernel* k)
{
    kernel = k;
    // need for alerts, lame
    container = kernel->getContainer();

    // these are now dynamically allocated to reduce header file dependencies
    midiRealizer.reset(new MidiRealizer());
    midiAnalyzer.reset(new MidiAnalyzer());
    hostAnalyzer.reset(new HostAnalyzer());
    pulsator.reset(new Pulsator(this));
    transport.reset(new Transport(this));

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
    pulsator->loadSession(s);
    transport->loadSession(s);
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
 * Only for TimeSlicer, don't need a list yet.
 */
void SyncMaster::addListener(Listener* l)
{
    listener = l;
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
    if (number == transportMaster) {
        transport->stop();
        // unlike notifyTrackReset, we get to keep being the transportMaster
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
    (void)number;
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
    Follower* f = pulsator->getFollower(number);
    if (f != nullptr) {
        // anything can become the track sync master
        if (trackSyncMaster == 0)
          trackSyncMaster = number;
        
        if (f->source == SyncSourceMaster) {
            // this one wants to be special
            if (transportMaster == 0) {

                connectTransport(number);
                
                transportMaster = number;

                // until we have manual options ready, auto start
                transport->start();
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
    TrackProperties props = tm->getTrackProperties(id);
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
 */
void SyncMaster::notifyTrackReset(int number)
{
    if (number == trackSyncMaster) {
        // Synchronizer used to choose a different one automatically
        // It looks like of confusing to see this still show as TrackSyncMaster in the UI
        // so reset it, but don't pick a new one
        trackSyncMaster = 0;
    }

    if (number == transportMaster) {
        // Synchronizer would send MIDI Stop at this point
        // it had a method fullStop that I think both sent the STOP event
        // and stop generating clocks
        transport->stop();

        transportMaster = 0;
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
    if (number == transportMaster) {
        // we don't need to distinguish between restructuring
        // and establishing a connection right now
        connectTransport(number);
    }
}

/**
 * Called when a track Restarts.
 * This can happen for several reasons:  The Start or Retrigger functions,
 * a Switch with the restart option, exiting Mute with the retrigger option,
 * the StartPoint function, etc.
 *
 * It means that the track made a jump back to the beginning rather than playing
 * to the beginning normally.  When this happens old Synchronizer had options
 * to send a MIDI Start event to keep external devices in sync.
 *
 * Now we inform the Transport which may choose to send MIDI Start.
 */
void SyncMaster::notifyTrackStart(int number)
{
    if (number == transportMaster) {
        transport->start();
    }
}

/**
 * Called when the track has entered a state of Pause.
 * This can happen with the Pause or GlobalPause functions, or the Stop
 * function which both pauses and rewinds.
 */
void SyncMaster::notifyTrackPause(int number)
{
    if (number == transportMaster) {

        // !! here is where we need to be a lot smarter about the difference
        // between MIDI Start and MIDI Continue, exiting a Pause does not
        // necessarily mean we send Start
        // The complication is that MIDI Continue requires a song position pointer,
        // and these are coarser grained than an audio stream frame location
        // the MIDI Continue would need to be delayed until the Transport actually
        // reaches that song position
        transport->stop();
    }
}

/**
 * Called when the track exists Pause.
 * See commentary in notifyTrackPause about why MIDI Continue is hard.
 */
void SyncMaster::notifyTrackResume(int number)
{
    if (number == transportMaster) {
        // probably wrong
        transport->start();
    }
}

/**
 * Called when a track enters Mute mode.
 *
 * Old Synchronizer had options to send a MIDI Stop event when this happened,
 * and then other options about what happened when the track unmuted.
 * Those should be moved to Transport parameters?  As it stands now, unmute
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
 * This is a special handler for some old MIDI event generation options.
 * Needs thought.
 */
void SyncMaster::notifyMidiStart(int number)
{
    (void)number;
}

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
 */
void SyncMaster::notifyTrackStop(int number)
{
    if (transportMaster == number) {
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
    if (transportMaster == number) {
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
	if (transportMaster == number) {
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

    Leader* leader = pulsator->getLeader(number);
    if (leader == nullptr)
      Trace(1, "SyncMaster: Invalid track id in SyncMasterTrack action");
    else {
        setTrackSyncMaster(number);
    }
}

/**
 * There can only be one Transport Master.
 * Changing this may change the tempo of geneerated MIDI clocks if the transport
 * has MIDI enabled.
 */
void SyncMaster::setTransportMaster(int id)
{
    if (transportMaster != id) {

        if (transportMaster > 0 && id == 0) {
            // unusual, they are asking to not have a sync master
            // what else should happen here?  Stop it?
            transport->disconnect();
        }

        if (id > 0)
          connectTransport(id);
        
        transportMaster = id;
    }
}

int SyncMaster::getTransportMaster()
{
    return transportMaster;
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

    Leader* leader = pulsator->getLeader(number);
    if (leader == nullptr)
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
    hostAnalyzer->advance(stream->getInterruptFrames());
}

void SyncMaster::refreshSampleRate(int rate)
{
    sampleRate = rate;
    
    hostAnalyzer->setSampleRate(rate);
    transport->setSampleRate(rate);
    midiRealizer->setSampleRate(rate);
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

    // host was advance in beginAudioBlock
    midiAnalyzer->advance(frames);

    // Transport should be controlling this but until it does it is important
    // to get the event queue consumed, Transport can just ask for the Result
    // when it advances
    midiRealizer->advance(frames);
    
    transport->advance(frames);

    // unless this needs the entire Stream, should take the frame count
    // like everything else, TMI
    pulsator->advance(stream);

    // see commentary about why this is complicated
    transport->checkDrift(frames);

    // Detect whether MIDI clocks have stopped comming in
    //
    // Supervisor formerly called this on the maintenance thread
    // interval, since we don't get a performMaintenance ping down here
    // we can check it on every block, the granularity doesn't really matter
    // since it is based off millisecond time advance
    midiAnalyzer->checkClocks();
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
    midiAnalyzer->enableEvents();
}

void SyncMaster::disableEventQueue()
{
    midiRealizer->disableEvents();
    midiAnalyzer->disableEvents();
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
    
    Symbol* s = a->symbol;

    switch (s->id) {

        case FuncSyncMasterTrack:
            setTrackSyncMaster(a);
            break;

        case FuncSyncMasterTransport:
            setTransportMaster(a);
            break;
        
        case FuncTransportStop:
            transport->userStop();
            break;

        case FuncTransportStart:
            transport->userStart();
            break;

        case ParamTransportTempo: {
            // Action doesn't have a way to pass floats right now so the
            // integer value is x100
            
            // !! if the Transport is locked to a Master track, this should be ignored
            float tempo = (float)(a->value) / 100.0f;
            transport->userSetTempo(tempo);
        }
            break;
            
        case ParamTransportLength: {
            // !! if the Transport is locked to a Master track, this should be ignored
            transport->userSetTempoDuration(a->value);
        }
            break;
            
        case ParamTransportBeatsPerBar:
            // this impacts more than just the Transport
            pulsator->userSetBeatsPerBar(a->value, true);
            break;
            
        default:
            // don't whine about this, we may be just one of several
            // potential LevelKernel action handlers
            //Trace(1, "SyncMaster: Unhandled action %s", s->getName());
            handled = false;
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
    bool success = false;
    Symbol* s = q->symbol;

    switch (s->id) {
        case ParamTransportTempo: {
            // no floats in Query yet...
            q->value = (int)(transport->getTempo() * 100.0f);
            success = true;
        }
            break;
            
        case ParamTransportBeatsPerBar: {
            q->value = transport->getBeatsPerBar();
            success = true;
        }
            break;
            
        default:
            Trace(1, "SyncMaster: Unhandled query %s", s->getName());
            break;
    }
    return success;
}

/**
 * Unclear whether we need to be including the state for the
 * non-transport sources.  Some of that will also be returned
 * through the TrackState for each track that follows a sync source.
 */
void SyncMaster::refreshState(SyncMasterState* extstate)
{
    transport->refreshState(extstate->transport);
    midiAnalyzer->refreshState(extstate->midi);
    hostAnalyzer->refreshState(extstate->host);

    extstate->transportMaster = transportMaster;
    extstate->trackSyncMaster = trackSyncMaster;
}

void SyncMaster::refreshPriorityState(PriorityState* pstate)
{
    transport->refreshPriorityState(pstate);
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
    container->addAlert(msg);
}

//////////////////////////////////////////////////////////////////////
//
// Granular State
//
//////////////////////////////////////////////////////////////////////

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
    
    Follower* f = getFollower(id);
    if (f != nullptr) {
        source = f->source;
        if (source == SyncSourceMaster) {
            if (transportMaster > 0 && transportMaster != id) {
                // there is already a transport master, this track
                // reverts to following the transport
                source = SyncSourceTransport;
            }
        }
    }
    return source;
}

/**
 * For the track monitoring UI, return information about the sync source a track
 * is following.
 */
float SyncMaster::getTempo(int trackNumber)
{
    float tempo = 0.0f;
    Follower* f = pulsator->getFollower(trackNumber);
    if (f != nullptr)
      tempo = varGetTempo(f->source);
    return tempo;
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

/**
 * HostAnalyzer has two beats, the one the host is telling us it's at and the
 * "normalized" beat derived from the synchronization unit.  Host beats are usually
 * unbounded raw beats that don't wrap on bar boundaries, though I think this is unspecified
 * and host dependent.
 *
 * Get beat/bar from the BarTenders in pulsator.
 * This is still messy.
 *
 * If we allow tracks to override the time signature, then these are of limited
 * use.  Some of the older core Variable handlers use it, but need to weed those out.
 */
int SyncMaster::varGetBeat(SyncSource src)
{
    int beat = 0;
    BarTender* tender = pulsator->getBarTender(src);
    if (tender != nullptr)
      beat = tender->getBeat();
    return beat;
}

int SyncMaster::varGetBar(SyncSource src)
{
    int bar = 0;
    BarTender* tender = pulsator->getBarTender(src);
    if (tender != nullptr)
      bar = tender->getBar();
    return bar;
}

int SyncMaster::varGetBeatsPerBar(SyncSource src)
{
    int bpb = 4;
    BarTender* tender = pulsator->getBarTender(src);
    if (tender != nullptr)
      bpb = tender->getBeatsPerBar();
    return bpb;
}

/**
 * The track specific time accessors are what should be used.
 */
int SyncMaster::getBeat(int number)
{
    int beat = 0;
    BarTender* tender = pulsator->getBarTender(number);
    if (tender != nullptr)
      beat = tender->getBeat();
    return beat;
}

int SyncMaster::getBar(int number)
{
    int bar = 0;
    BarTender* tender = pulsator->getBarTender(number);
    if (tender != nullptr)
      bar = tender->getBar();
    return bar;
}

int SyncMaster::getBeatsPerBar(int number)
{
    int bpb = 4;
    BarTender* tender = pulsator->getBarTender(number);
    if (tender != nullptr)
      bpb = tender->getBeatsPerBar();
    return bpb;
}

//////////////////////////////////////////////////////////////////////
//
// Leader/Follower
//
//////////////////////////////////////////////////////////////////////

Follower* SyncMaster::getFollower(int id)
{
    return pulsator->getFollower(id);
}

/**
 * Following a sync source does not result in track dependencies so you
 * don't need to inform TimeSlicer and reorder.
 */
void SyncMaster::follow(int follower, SyncSource source, SyncUnit unit)
{
    pulsator->follow(follower, source, unit);
}

/**
 * Adding a follower relationship changes dependency order that
 * TimeSlicer is using to guide the track advance.  Let it know
 */
void SyncMaster::follow(int follower, int leader, SyncUnit unit)
{
    pulsator->follow(follower, leader, unit);

    if (listener != nullptr)
      listener->syncFollowerChanges();
}

/**
 * Unfollowing doesn't change dependency order, though it may loosen it.
 * This could resolve dependency cycles if we reorrder now.  Not worth bothering.
 */
void SyncMaster::unfollow(int follower)
{
    pulsator->unfollow(follower);
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
// Transport/MIDI Out
//
// These are old and should only be used for some core script Variables.
// Weed these out in time.
// I put "var" in front of them to make it clear what they're intended for
// the rest of the system shouldn't be using these.
//
//////////////////////////////////////////////////////////////////////

//
// This is all temporary, MIDI out status will be the same thing
// as the Transport status, don't think we need those to be independent
// 

bool SyncMaster::varIsMidiOutSending()
{
    return midiRealizer->isSending();
}
bool SyncMaster::varIsMidiOutStarted()
{
    return midiRealizer->isStarted();
}
int SyncMaster::varGetMidiOutStarts()
{
    return midiRealizer->getStarts();
}
int SyncMaster::varGetMidiOutSongClock()
{
    return midiRealizer->getSongClock();
}
int SyncMaster::varGetMidiOutRawBeat()
{
    return midiRealizer->getBeat();
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Input
//
//////////////////////////////////////////////////////////////////////
    
float SyncMaster::varGetMidiInTempo()
{
    return midiAnalyzer->getTempo();
}
int SyncMaster::varGetMidiInSmoothTempo()
{
    return midiAnalyzer->getSmoothTempo();
}
int SyncMaster::varGetMidiInRawBeat()
{
    return midiAnalyzer->getBeat();
}
int SyncMaster::varGetMidiInSongClock()
{
    return midiAnalyzer->getSongClock();
}
bool SyncMaster::varIsMidiInReceiving()
{
    return midiAnalyzer->isReceiving();
}
bool SyncMaster::varIsMidiInStarted()
{
    return midiAnalyzer->isStarted();
}

//////////////////////////////////////////////////////////////////////
//
// Host
//
//////////////////////////////////////////////////////////////////////
    
bool SyncMaster::varIsHostReceiving()
{
    //return midiAnalyzer->isReceiving();
    return false;
}
bool SyncMaster::varIsHostStarted()
{
    //return midiAnalyzer->isStarted();
    return false;
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
