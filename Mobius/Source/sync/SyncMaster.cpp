
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
#include "SyncMasterState.h"

#include "SyncMaster.h"

SyncMaster::SyncMaster()
{
}

SyncMaster::~SyncMaster()
{
}

/**
 * The event queue should only be enabled once audio blocks
 * start comming in.  If blocks stop then the queue can overflow
 * if there is MIDI being actively received or sent.
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

void SyncMaster::shutdown()
{
    midiRealizer->shutdown();
}

void SyncMaster::kludgeSetup(MobiusKernel* k, MidiManager* mm)
{
    kernel = k;

    midiRealizer.reset(new MidiRealizer());
    midiAnalyzer.reset(new MidiAnalyzer());
    pulsator.reset(new Pulsator(this));

    midiRealizer->initialize(this, mm);
    midiAnalyzer->initialize(this, mm);
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

    // once we start receiving audio blocks, it is okay to start converting
    // MIDI events into MidiSyncMessages, if you allow event queueing before
    // blocks come in, the queue can overflow
    // todo: while not normally a problem, if you disconnect the audio stream
    // after initialization, then the queue will get stuck and overflow, the maintenance
    // thread could monitor for a suspension of audio blocks and disable the queue
    enableEventQueue();
    
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
    midiAnalyzer->checkClocks();
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

void SyncMaster::refreshState(SyncMasterState* extstate)
{
    // todo: if we consolidate all of the state sources up here
    // we could just to a single structure copy

    extstate->transport = state.transport;

    midiAnalyzer->getState(extstate->midi);
}

void SyncMaster::refreshPriorityState(PriorityState* pstate)
{
    transport.refreshPriorityState(pstate);
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

//
// This is all temporary, MIDI out status will be the same thing
// as the Transport status, don't think we need those to be independent
// 

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
    return midiRealizer->getBeat();
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
    return midiRealizer->nextEvent();
}
class MidiSyncEvent* SyncMaster::midiOutIterateNext()
{
    return midiRealizer->iterateNext();
}
void SyncMaster::midiOutIterateStart()
{
    return midiRealizer->iterateStart();
}

//////////////////////////////////////////////////////////////////////
//
// MIDI Input
//
//////////////////////////////////////////////////////////////////////
    
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
    return midiAnalyzer->iterateStart();
}


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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
