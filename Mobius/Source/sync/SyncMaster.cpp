
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
#include "SyncMasterState.h"

#include "SyncMaster.h"

SyncMaster::SyncMaster()
{
}

SyncMaster::~SyncMaster()
{
}

void SyncMaster::enableEvents()
{
    midiRealizer->enableEvents();
}

void SyncMaster::shutdown()
{
    midiRealizer->shutdown();
}

void SyncMaster::kludgeSetup(MobiusKernel* k, MidiManager* mm)
{
    kernel = k;
    
    midiRealizer.reset(new MidiRealizer());
    pulsator.reset(new Pulsator());

    pulsator->kludgeSetup(this, midiRealizer.get());
    midiRealizer->kludgeSetup(this, mm);
}

void SyncMaster::setSampleRate(int rate)
{
    transport.setSampleRate(rate);
    midiRealizer->setSampleRate(rate);
}

void SyncMaster::sendAlert(juce::String msg)
{
    // MidiRealizer does this and used to call supervisor->addAlert
    kernel->sendAlert(msg);
}

void SyncMaster::loadSession(Session* s)
{
    pulsator->loadSession(s);
}

/**
 * Called by kernel components to see the transport pulse this block.
 */
void SyncMaster::getTransportPulse(Pulse& p)
{
    p = transportPulse;
}

/**
 * This is in about a dozen places now.  Formerly tried to force this
 * through MobiusContainer but we don't have that here, and I'm tired
 * of hiding Juce.
 *
 * Within the SyncMaster subcomponents, it would be good to have a stable
 * millisecond number that is captured at the start of each audio block rather
 * than going back to Juce each time, which might come back with a different number.
 */
int SyncMaster::getMilliseconds()
{
    return juce::Time::getMillisecondCounter();
}

//////////////////////////////////////////////////////////////////////
//
// Actions
//
//////////////////////////////////////////////////////////////////////

void SyncMaster::doAction(UIAction* a)
{
    Symbol* s = a->symbol;

    switch (s->id) {
        case FuncTransportStop: doStop(a); break;
        case FuncTransportStart: doStart(a); break;
        case ParamTransportTempo: doTempo(a); break;
        case ParamTransportBeatsPerBar: doBeatsPerBar(a); break;
        default:
            Trace(1, "SyncMaster: Unhandled action %s", s->getName());
            break;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Query
//
//////////////////////////////////////////////////////////////////////

bool SyncMaster::doQuery(Query* q)
{
    bool success = false;
    Symbol* s = q->symbol;

    switch (s->id) {
        case ParamTransportTempo: {
            // no floats in Query yet...
            q->value = (int)(transport.getTempo() * 100.0f);
            success = true;
        }
            break;
            
        case ParamTransportBeatsPerBar: {
            q->value = transport.getBeatsPerBar();
            success = true;
        }
            break;
            
        default:
            Trace(1, "SyncMaster: Unhandled query %s", s->getName());
            break;
    }
    return success;
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

void SyncMaster::advance(MobiusAudioStream* stream)
{
    int frames = stream->getInterruptFrames();
    
    if (transport.advance(frames, transportPulse)) {
        transportPulse.source = Pulse::SourceTransport;
    }
    else {
        // dumb way to indiciate "none"
        transportPulse.source = Pulse::SourceNone;
    }

    pulsator->interruptStart(stream);

    // Supervisor formerly called this on the maintenance thread
    // interval, since we don't get a performMaintenance ping down here
    // we can check it on every block, the granularity doesn't really matter
    // since it is based off millisecond time advance
    midiRealizer->checkClocks();
}

//////////////////////////////////////////////////////////////////////
//
// Transport Actions
//
//////////////////////////////////////////////////////////////////////

void SyncMaster::doStop(UIAction* a)
{
    (void)a;
}

void SyncMaster::doStart(UIAction* a)
{
    (void)a;
}

void SyncMaster::doTempo(UIAction* a)
{
    (void)a;
}

void SyncMaster::doBeatsPerBar(UIAction* a)
{
    (void)a;
}

//////////////////////////////////////////////////////////////////////
//
// State
//
//////////////////////////////////////////////////////////////////////

void SyncMaster::refreshState(SyncMasterState* state)
{
    state->tempo = transport.getTempo();
    state->beatsPerBar = transport.getBeatsPerBar();
    state->beat = transport.getBeat();
    state->bar = transport.getBar();
}

void SyncMaster::refreshPriorityState(PriorityState* state)
{
    transport.refreshPriorityState(state);
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
float SyncMaster::getTempo(Pulse::Source src)
{
    return pulsator->getTempo(src);
}
int SyncMaster::getBeat(Pulse::Source src)
{
    return pulsator->getBeat(src);
}
int SyncMaster::getBar(Pulse::Source src)
{
    return pulsator->getBar(src);
}
int SyncMaster::getBeatsPerBar(Pulse::Source src)
{
    return pulsator->getBeatsPerBar(src);
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Out
//
//////////////////////////////////////////////////////////////////////

float SyncMaster::getMidiOutTempo()
{
    return midiRealizer->getTempo();
}
void SyncMaster::setMidiOutTempo(float tempo)
{
    midiRealizer->setTempo(tempo);
}
int SyncMaster::getMidiOutRawBeat()
{
    return midiRealizer->getRawBeat();
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
    midiRealizer->midiContinue();
}
class MidiSyncEvent* SyncMaster::midiOutNextEvent()
{
    return midiRealizer->nextOutputEvent();
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Input
//
//////////////////////////////////////////////////////////////////////
    
class MidiSyncEvent* SyncMaster::midiInNextEvent()
{
    return midiRealizer->nextInputEvent();
}
float SyncMaster::getMidiInTempo()
{
    return midiRealizer->getInputTempo();
}
int SyncMaster::getMidiInSmoothTempo()
{
    return midiRealizer->getInputSmoothTempo();
}
int SyncMaster::getMidiInRawBeat()
{
    return midiRealizer->getInputRawBeat();
}
int SyncMaster::getMidiInSongClock()
{
    return midiRealizer->getInputSongClock();
}
bool SyncMaster::isMidiInReceiving()
{
    return midiRealizer->isInputReceiving();
}
bool SyncMaster::isMidiInStarted()
{
    return midiRealizer->isInputStarted();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
