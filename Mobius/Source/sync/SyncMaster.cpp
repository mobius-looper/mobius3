
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/Symbol.h"
#include "../model/Session.h"
#include "../model/UIAction.h"
#include "../model/Query.h"
#include "../model/PriorityState.h"

#include "../mobius/MobiusKernel.h"

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
void SyncMaster::initialize(MobiusContainer* c)
{
    container = c;

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

    midiAnalyzer->getState(extstate->midi);
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

        case Pulse::SourceMidiIn: {
            // Pulsator also tracks this but we can get it directly from the Analyzer
            tempo = midiAnalyzer->getTempo();
        }
            break;

        case Pulse::SourceMidiOut:
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

        case Pulse::SourceMidiIn: {
            // Pulsator also tracks this but we can get it directly from the Analyzer
            beat = midiAnalyzer->getBeat();
        }
            break;

        case Pulse::SourceMidiOut:
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

        case Pulse::SourceMidiIn: {
            int beat = midiAnalyzer->getBeat();
            int bpb = getBeatsPerBar(src);
            if (bpb > 0)
              bar = (beat / bpb) + 1;
        }
            break;

        case Pulse::SourceMidiOut:
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

//////////////////////////////////////////////////////////////////////
//
// Leader/Follower
//
//////////////////////////////////////////////////////////////////////

void SyncMaster::addLeaderPulse(int leader, Pulse::Type type, int frameOffset)
{
    pulsator->addLeaderPulse(leader, type, frameOffset);
}

void SyncMaster::follow(int follower, Pulse::Source source, Pulse::Type type)
{
    pulsator->follow(follower, source, type);
}
void SyncMaster::follow(int follower, int leader, Pulse::Type type)
{
    pulsator->follow(follower, leader, type);
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
void SyncMaster::unfollow(int follower)
{
    pulsator->unfollow(follower);
}
void SyncMaster::setOutSyncMaster(int leaderId, int leaderFrames)
{
    pulsator->setOutSyncMaster(leaderId, leaderFrames);
}
int SyncMaster::getOutSyncMaster()
{
    return pulsator->getOutSyncMaster();
}
void SyncMaster::setTrackSyncMaster(int leader, int leaderFrames)
{
    pulsator->setTrackSyncMaster(leader, leaderFrames);
}
int SyncMaster::getTrackSyncMaster()
{
    return pulsator->getTrackSyncMaster();
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
int SyncMaster::getPulseFrame(int follower)
{
    return pulsator->getPulseFrame(follower);
}
int SyncMaster::getPulseFrame(int followerId, Pulse::Type type)
{
    return pulsator->getPulseFrame(followerId, type);
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
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
