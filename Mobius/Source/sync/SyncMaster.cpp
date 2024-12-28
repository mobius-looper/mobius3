
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/Symbol.h"
#include "../model/Session.h"
#include "../model/UIAction.h"
#include "../model/Query.h"
#include "../model/PriorityState.h"

#include "../Provider.h"

#include "Pulsator.h"
#include "Pulse.h"
#include "SyncMasterState.h"

#include "SyncMaster.h"

SyncMaster::SyncMaster()
{
}

SyncMaster::~SyncMaster()
{
}

void SyncMaster::setSampleRate(int rate)
{
    transport.setSampleRate(rate);
}

/**
 * Called by kernel components to see the transport pulse this block.
 */
void SyncMaster::getTransportPulse(Pulse& p)
{
    p = transportPulse;
}

//////////////////////////////////////////////////////////////////////
//
// Sessions
//
//////////////////////////////////////////////////////////////////////

void SyncMaster::loadSession(Session* s)
{
    (void)s;
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

void SyncMaster::advance(int frames)
{
    if (transport.advance(frames, transportPulse)) {
        transportPulse.source = Pulse::SourceTransport;
    }
    else {
        // dumb way to indiciate "none"
        transportPulse.source = Pulse::SourceNone;
    }
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
