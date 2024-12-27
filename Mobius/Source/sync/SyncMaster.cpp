
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
        case FuncMetronomeStop: doStop(a); break;
        case FuncMetronomeStart: doStart(a); break;
        case ParamMetronomeTempo: doTempo(a); break;
        case ParamMetronomeBeatsPerBar: doBeatsPerBar(a); break;
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
        case ParamMetronomeTempo: {
            // no floats in Query yet...
            q->value = (int)(transport.getTempo() * 100.0f);
            success = true;
        }
            break;
            
        case ParamMetronomeBeatsPerBar: {
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
    (void)frames;
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
