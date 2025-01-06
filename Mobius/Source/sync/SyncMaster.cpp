
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
    // need this for the sample rate and alerts
    container = kernel->getContainer();

    // these are now dynamically allocated to reduce header file dependencies
    midiRealizer.reset(new MidiRealizer());
    midiAnalyzer.reset(new MidiAnalyzer());
    pulsator.reset(new Pulsator(this));
    transport.reset(new Transport(this));

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
 * This is called when a track is reset, if this was one of the sync masters
 * Synchronizer would try to auto-assign another one.
 *
 * This unlocks the follower automatically.
 *
 * If this was the TrackSyncMaster, this is where the old Synchronizer would choose
 * a new one.  Unclear how I want this to work.  We would have to pick one at random
 * which is hard to predict.   Wait until the next new recording which calls
 * notifyTrackAvailable or make the user do it manually.
 *
 * todo: If this is the TransportMaster then Synchronizer is currently calling
 * SyncMaster::midiOutStop.  There are a lot of calls to isTransportMaster down
 * there to send MIDI events.  Those should all be moved up here so audio and MIDI
 * tracks behave the ssame.
 */
void SyncMaster::notifyTrackReset(int number)
{
    pulsator->unlock(number);

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
        midiRealizer->stop();

        transportMaster = 0;
    }
}

/**
 * This is called when a track begins recording.
 * If this is the TransportMaster, Synchronizer in the past would do a "full stop"
 * to send a STOP event and stop sending MIDI clocks.
 */
void SyncMaster::notifyTrackRecord(int number)
{
    // continue calling MidiRealizer but this needs to be under the control of the Transport
    if (number == transportMaster) {
        midiRealizer->stop();
        // unlike notifyTrackReset, we get to keep being the transportMaster
    }
}

/**
 * Called after a loop or project load in a track.
 * If the track now has content and there was no track out out sync master,
 * Synchronizer would auto-assign this one.
 */
void SyncMaster::notifyLoopLoad(int number)
{
    (void)number;
}

/**
 * This is supposed to be called when any track has finished recording
 * and could serve as the track sync master.
 *
 * If there is already a sync master, it is not changed, though we should
 * allow a special sync mode, maybe Pulse::SourceMasterForce or some other
 * parameter that overrides it.
 *
 * Also worth considering if we need an option for tracks to not become the
 * track sync master if they don't want to.
 */
void SyncMaster::notifyTrackAvailable(int number)
{
    // verify the number is in range and can be a leader
    Follower* f = pulsator->getFollower(number);
    if (f == nullptr) {
        Trace(1, "Synchronizer: Invalid track number %d", number);
    }
    else {
        // anything can become the track sync master
        if (trackSyncMaster == 0)
          trackSyncMaster = number;
        
        if (f->source == Pulse::SourceMaster) {
            // this one wants to be special
            if (transportMaster == 0) {

                TrackManager* tm = kernel->getTrackManager();
                TrackProperties props = tm->getTrackProperties(number);
                // todo: use the frame count to set the transport tempo
                transportMaster = number;
            }
        }
    }
}

/**
 * Called by a track when it the loop it is playing changes in some way
 * that may impact the generated MIDI clocks if it is the TransportMaster
 * and the tempo follows the loop length.
 *
 * Examples include Multiply, Insert, Divide, Undo/Redo.
 *
 * Now also calling this for loop switch, which used the old option
 * ResizeSyncAdjust from the Setup to control whether the tempo changed or not.
 * 
 */
void SyncMaster::notifyTrackResize(int number)
{
    if (transportMaster == number) {

        // todo: change tempo
    }
}

/**
 * Called by a track when it has jumped playback back to the beginning.
 * Mobius does this after Unrounded Multiply and Trim, in combination
 * with notifyTrackResize.
 *
 * If this is the TransportMaster we have historically sent a MIDI Start event.
 */
void SyncMaster::notifyTrackRestart(int number, bool force)
{
    if (transportMaster == number) {
        bool checkManual = !force;
        sendStart(checkManual, false);
    }
}

/**
 * Called by a track when the playback rate changes.
 * This has adjusted the generated clock tempo controlled by
 * the Setup parameter speedSyncAdjust
 */
void SyncMaster::notifyTrackSpeed(int number)
{
    if (transportMaster == number) {

        //Setup* setup = mMobius->getActiveSetup();
        //SyncAdjust mode = setup->getSpeedSyncAdjust();
        //if (mode == SYNC_ADJUST_TEMPO)
    }
}

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
			incMidiOutStarts();
        }
        else {
            Trace(2, "SyncMaster: Sending MIDI Start\n");
            // mTransport->start(l);
            midiOutStart();
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
    midiOutStopSelective(doTransport, doClocks);
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
            midiOutContinue();
        }
        else  {
            // we just stopped sending clocks, resume them
            // mTransport->startClocks(l);
            midiOutStartClocks();
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

//////////////////////////////////////////////////////////////////////
//
// Masters
//
//////////////////////////////////////////////////////////////////////

/**
 * There can be one TrackSyncMaster.
 *
 * This becomes the default leader track when using Pulse::SourceLeader
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
    transportMaster = id;

    // todo: this has lots of consequences, we're going to need to query
    // the track to get it's length and possibly other things to control
    // the Transport
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

    // monitor changes to the sample rate once the audio device is pumping
    // and adjust internal calculations
    int newSampleRate = container->getSampleRate();
    if (newSampleRate != sampleRate)
      refreshSampleRate(sampleRate);
    
    transport->advance(frames);

    pulsator->interruptStart(stream);

    // Supervisor formerly called this on the maintenance thread
    // interval, since we don't get a performMaintenance ping down here
    // we can check it on every block, the granularity doesn't really matter
    // since it is based off millisecond time advance
    midiAnalyzer->checkClocks();
}

void SyncMaster::refreshSampleRate(int rate)
{
    sampleRate = rate;
    transport->setSampleRate(rate);
    midiRealizer->setSampleRate(rate);
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
void SyncMaster::doAction(UIAction* a)
{
    Symbol* s = a->symbol;

    switch (s->id) {

        case FuncSyncMasterTrack:
            setTrackSyncMaster(a);
            break;

        case FuncSyncMasterTransport:
            setTransportMaster(a);
            break;
        
        case FuncTransportStop:
            transport->stop();
            break;

        case FuncTransportStart:
            transport->start();
            break;

            // todo FuncTransportPause
            
        case ParamTransportTempo: {
            // Action doesn't have a way to pass floats right now so the
            // integer value is x100
            float tempo = (float)(a->value) / 100.0f;
            transport->setTempo(tempo);
        }
            break;
            
        case ParamTransportBeatsPerBar:
            transport->setBeatsPerBar(a->value);
            break;

        default:
            Trace(1, "SyncMaster: Unhandled action %s", s->getName());
            break;
    }
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
    extstate->transport = states.transport;
    extstate->host = states.host;

    // Analyzer maintains it's own fields, it doesn't use SyncSourceState
    midiAnalyzer->getState(extstate->midi);

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
 * For the track monitoring UI, return information about the sync source this
 * track is following.
 *
 * For MIDI do we want to return the fluctuationg tempo or smooth tempo
 * with only one decimal place?
 */
float SyncMaster::getTempo(Pulse::Source src)
{
    float tempo = 0.0f;
    switch (src) {
        
        case Pulse::SourceHost: {
            // Pulsator tracks this
            Pulsator::SyncState* state = pulsator->getHostState();
            tempo = state->tempo;
        }
            break;

        case Pulse::SourceMidi: {
            // Pulsator also tracks this but we can get it directly from the Analyzer
            tempo = midiAnalyzer->getTempo();
        }
            break;

        case Pulse::SourceMaster:
        case Pulse::SourceTransport: {
            // these are now the same
            tempo = transport->getTempo();
        }
            break;

        default: break;
    }
    return tempo;
}

/**
 * Todo: old code had the notion of "raw beat" which advanced without wrapping
 * and regular beat which wrapped according to beatsPerBar.
 *
 * This currently returns raw beats for MidiIn.  May need to differentiate this.
 */
int SyncMaster::getBeat(Pulse::Source src)
{
    int beat = 0;
    switch (src) {
    
        case Pulse::SourceHost: {
            // Pulsator tracks this
            Pulsator::SyncState* state = pulsator->getHostState();
            beat = state->beat;
        }
            break;

        case Pulse::SourceMidi: {
            // Pulsator also tracks this but we can get it directly from the Analyzer
            beat = midiAnalyzer->getBeat();
        }
            break;

        case Pulse::SourceMaster:
        case Pulse::SourceTransport: {
            // these are now the same
            beat = transport->getBeat();
        }
            break;

        default: break;
    }
    return beat;
}

/**
 * The host usually has a reliable time signature but not always.
 * MIDI doesn't have a time signature at all.
 * The Transport has one under user control.
 *
 * Fall back to the Transport which will get it from the Session.
 *
 * In old code, the BPB for internal tracks was annoyingly complex, getting it from
 * the Setup or the current value of the subcycles parameter.  Mobius core
 * can continue doing that if it wants, but it would be best to standardize
 * on getting it from the Transport.
 */
int SyncMaster::getBeatsPerBar(Pulse::Source src)
{
    int bpb = transport->getBeatsPerBar();

    if (src == Pulse::SourceHost) {
        Pulsator::SyncState* state = pulsator->getHostState();
        int hbpb = state->beatsPerBar;
        if (hbpb > 0)
          bpb = hbpb;
    }
    return bpb;
}

/**
 * What bar you're on depends on an accurate value for beatsPerBar.
 * Transport will have this and host usually will, but we have to guess
 * for MIDI.
 */
int SyncMaster::getBar(Pulse::Source src)
{
    // is this supposed to be zero relative like beat?
    // older code assumed 1 relative
    int bar = 1;
    switch (src) {
    
        case Pulse::SourceHost: {
            // Pulsator tracks this
            Pulsator::SyncState* state = pulsator->getHostState();
            bar = state->bar;
        }
            break;

        case Pulse::SourceMidi: {
            int beat = midiAnalyzer->getBeat();
            int bpb = getBeatsPerBar(src);
            if (bpb > 0)
              bar = (beat / bpb) + 1;
        }
            break;

        case Pulse::SourceMaster:
        case Pulse::SourceTransport: {
            // these are now the same
            bar = transport->getBar();
        }

        default: break;
    }
    return bar;
}

int SyncMaster::getMasterBarFrames()
{
    return transport->getMasterBarFrames();
}

/**
 * This is the main way that the old Synchronizer injects sync events
 * into the Track event timeline.
 */
Pulse* SyncMaster::getBlockPulse(Pulse::Source src)
{
    return pulsator->getBlockPulse(src);
}

Pulse* SyncMaster::getBlockPulse(Follower* f)
{
    return pulsator->getBlockPulse(f);
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
 * Adding a follower relationship changes dependency order that
 * TimeSlicer is using to guide the track advance.  Let it know
 */
void SyncMaster::follow(int follower, int leader, Pulse::Type type)
{
    pulsator->follow(follower, leader, type);

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
void SyncMaster::addLeaderPulse(int leader, Pulse::Type type, int frameOffset)
{
    pulsator->addLeaderPulse(leader, type, frameOffset);
}

/**
 * Following a sync source does not result in track dependencies so you
 * don't need to inform TimeSlicer and reorder.
 */
void SyncMaster::follow(int follower, Pulse::Source source, Pulse::Type type)
{
    pulsator->follow(follower, source, type);
}

void SyncMaster::start(int follower)
{
    pulsator->start(follower);
}
void SyncMaster::lock(int follower, int frames)
{
    pulsator->lock(follower, frames);
}
void SyncMaster::unlock(int follower)
{
    pulsator->unlock(follower);
}
bool SyncMaster::shouldCheckDrift(int follower)
{
    return pulsator->shouldCheckDrift(follower);
}
int SyncMaster::getDrift(int follower)
{
    return pulsator->getDrift(follower);
}
void SyncMaster::correctDrift(int follower, int frames)
{
    pulsator->correctDrift(follower, frames);
}

//////////////////////////////////////////////////////////////////////
//
// Transport/MIDI Out
//
//////////////////////////////////////////////////////////////////////

//
// This is all temporary, MIDI out status will be the same thing
// as the Transport status, don't think we need those to be independent
// 

float SyncMaster::getTempo()
{
    return transport->getTempo();
}
void SyncMaster::setTempo(float tempo)
{
    transport->setTempo(tempo);
}

bool SyncMaster::isMidiOutSending()
{
    return midiRealizer->isSending();
}
bool SyncMaster::isMidiOutStarted()
{
    return midiRealizer->isStarted();
}
int SyncMaster::getMidiOutStarts()
{
    return midiRealizer->getStarts();
}
void SyncMaster::incMidiOutStarts()
{
    midiRealizer->incStarts();
}
int SyncMaster::getMidiOutSongClock()
{
    return midiRealizer->getSongClock();
}
int SyncMaster::getMidiOutRawBeat()
{
    return midiRealizer->getBeat();
}
void SyncMaster::midiOutStart()
{
    midiRealizer->start();
}
void SyncMaster::midiOutStartClocks()
{
    midiRealizer->startClocks();
}
void SyncMaster::midiOutStop()
{
    midiRealizer->stop();
}
void SyncMaster::midiOutStopSelective(bool sendStop, bool stopClocks)
{
    midiRealizer->stopSelective(sendStop, stopClocks);
}
void SyncMaster::midiOutContinue()
{
    // todo: several issues here, what does this mean for transport?
    // what does pause() do?
    midiRealizer->midiContinue();
}

// Synchronizer needs to be using pulses now
#if 0
/**
 * The event interface should no longer be used.  The clock generator
 * thread will be adjusted to match the advance of the transport bar, not the
 * other way around.  Still needed for legacy code in Synchronizer.
 */
class MidiSyncEvent* SyncMaster::midiOutNextEvent()
{
    return midiRealizer->nextEvent();
}
class MidiSyncEvent* SyncMaster::midiOutIterateNext()
{
    return midiRealizer->iterateNext();
}
void SyncMaster::midiOutIterateStart()
{
    midiRealizer->iterateStart();
}
#endif

//////////////////////////////////////////////////////////////////////
//
// MIDI Input
//
//////////////////////////////////////////////////////////////////////
    
#if 0
class MidiSyncEvent* SyncMaster::midiInNextEvent()
{
    return midiAnalyzer->nextEvent();
}
class MidiSyncEvent* SyncMaster::midiInIterateNext()
{
    return midiAnalyzer->iterateNext();
}
void SyncMaster::midiInIterateStart()
{
    midiAnalyzer->iterateStart();
}
#endif

float SyncMaster::getMidiInTempo()
{
    return midiAnalyzer->getTempo();
}
int SyncMaster::getMidiInSmoothTempo()
{
    return midiAnalyzer->getSmoothTempo();
}
int SyncMaster::getMidiInRawBeat()
{
    return midiAnalyzer->getBeat();
}
int SyncMaster::getMidiInSongClock()
{
    return midiAnalyzer->getSongClock();
}
bool SyncMaster::isMidiInReceiving()
{
    return midiAnalyzer->isReceiving();
}
bool SyncMaster::isMidiInStarted()
{
    return midiAnalyzer->isStarted();
}

//////////////////////////////////////////////////////////////////////
//
// Host
//
//////////////////////////////////////////////////////////////////////
    
bool SyncMaster::isHostReceiving()
{
    //return midiAnalyzer->isReceiving();
    return false;
}
bool SyncMaster::isHostStarted()
{
    //return midiAnalyzer->isStarted();
    return false;
}

#if 0
class MidiSyncEvent* SyncMaster::hostNextEvent()
{
    //return midiAnalyzer->nextEvent();
    return nullptr;
}
class MidiSyncEvent* SyncMaster::hostIterateNext()
{
    //return midiAnalyzer->iterateNext();
    return nullptr;
}
void SyncMaster::hostIterateStart()
{
    //midiAnalyzer->iterateStart();
}

float SyncMaster::getHostTempo()
{
    return states.host.tempo;
}
int SyncMaster::getHostBeat()
{
    return states.host.beat;
}

#endif

//////////////////////////////////////////////////////////////////////
//
// Bar Lengths
//
//////////////////////////////////////////////////////////////////////

/**
 * Old code scraped from Synchronizer, we need to do something similar here.
 */
int SyncMaster::getBarFrames(Pulse::Source src)
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
