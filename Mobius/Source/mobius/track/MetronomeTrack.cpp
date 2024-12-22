
#include <JuceHeader.h>

#include "../../model/Symbol.h"
#include "../../model/Session.h"
#include "../../model/UIAction.h"
#include "../../model/Query.h"
#include "../../model/MobiusState.h"

#include "../MobiusInterface.h"
#include "../Notification.h"

#include "TrackProperties.h"
#include "TrackManager.h"
#include "LogicalTrack.h"
#include "BaseTrack.h"
#include "MetronomeTrack.h"

MetronomeTrack::MetronomeTrack(TrackManager* tm, LogicalTrack* lt) : BaseTrack(tm, lt)
{
}

MetronomeTrack::~MetronomeTrack()
{
}

//////////////////////////////////////////////////////////////////////
//
// BaseTrack Implementations
//
//////////////////////////////////////////////////////////////////////

void MetronomeTrack::loadSession(Session::Track* def)
{
    (void)def;
}

void MetronomeTrack::doAction(UIAction* a)
{
    Symbol* s = a->symbol;
    switch (s->id) {
        case FuncMetronomeStop: doStop(a); break;
        case FuncMetronomeStart: doStart(a); break;
    }
}

bool MetronomeTrack::doQuery(Query* q)
{
    (void)q;
    return false;
}

void MetronomeTrack::processAudioStream(MobiusAudioStream* stream)
{
    advance(stream->getInterruptFrames());
}

void MetronomeTrack::midiEvent(MidiEvent* e)
{
    (void)e;
}

void MetronomeTrack::getTrackProperties(TrackProperties& props)
{
    (void)props;
}

void MetronomeTrack::trackNotification(NotificationId notification, TrackProperties& props)
{
    (void)notification;
    (void)props;
}

int MetronomeTrack::getGroup()
{
    return 0;
}

bool MetronomeTrack::isFocused()
{
    return false;
}

void MetronomeTrack::refreshPriorityState(MobiusState::Track* tstate)
{
    (void)tstate;
}

void MetronomeTrack::refreshState(MobiusState::Track* tstate)
{
    (void)tstate;
}

void MetronomeTrack::dump(StructureDumper& d)
{
    (void)d;
}

class MslTrack* MetronomeTrack::getMslTrack()
{
    return nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// Functions
//
//////////////////////////////////////////////////////////////////////

void MetronomeTrack::doStop(UIAction* a)
{
    (void)a;
}

void MetronomeTrack::doStart(UIAction* a)
{
    (void)a;
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

void MetronomeTrack::advance(int frames)
{
    (void)frames;
}
