
#include <JuceHeader.h>

#include "../../model/ObjectPool.h"

#include "TrackEvent.h"

//////////////////////////////////////////////////////////////////////
//
// Event
//
//////////////////////////////////////////////////////////////////////

TrackEvent::TrackEvent()
{
}

TrackEvent::~TrackEvent()
{
}

/**
 * Pool cleanser
 */
void TrackEvent::init()
{
    next = nullptr;
    type = EventNone;
    frame = 0;
}

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

TrackEventPool::TrackEventPool()
{
    setName("TrackEvent");
    fluff();
}

TrackEventPool::~TrackEventPool()
{
}

/**
 * ObjectPool overload to create a new pooled object.
 */
PooledObject* TrackEventPool::alloc()
{
    return new TrackEvent();
}

/**
 * Accessor for most of the code that does the convenient downcast.
 */
TrackEvent* TrackEventPool::newEvent()
{
    return (TrackEvent*)checkout();
}

//////////////////////////////////////////////////////////////////////
//
// List
//
//////////////////////////////////////////////////////////////////////

TrackEventList::TrackEventList()
{
}

TrackEventList::~TrackEventList()
{
    flush();
}

void TrackEventList::initialize(TrackEventPool* p)
{
    pool = p;
}

void TrackEventList::flush()
{
    while (events != nullptr) {
        TrackEvent* next = events->next;
        events->next = nullptr;
        pool->checkin(events);
        events = next;
    }
}

void TrackEventList::add(TrackEvent* e)
{
    (void)e;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
