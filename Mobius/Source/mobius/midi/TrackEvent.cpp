
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
    pending = false;
    pulsed = false;
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

void TrackEventList::add(TrackEvent* e, bool priority)
{
    if (e->pending) {
        // straight to the end
        TrackEvent* last = events;
        while (last != nullptr && last->next != nullptr)
          last = last->next;
        if (last == nullptr)
          events = e;
        else
          last->next = e;
    }
    else {
        TrackEvent* prev = nullptr;
        TrackEvent* next = events;

        // the start of the events on or after this frame
        while (next != nullptr && (next->pending || next->frame < e->frame)) {
            prev = next;
            next = next->next;
        }

        // priority events go in the front of this frame, otherwise the end
        if (!priority) {
            while (next != nullptr && (next->pending || next->frame == e->frame)) {
                prev = next;
                next = next->next;
            }
        }
        
        if (prev == nullptr) {
            e->next = events;
            events = e;
        }
        else {
            e->next = prev->next;
            prev->next = e;
        }
    }
}

TrackEvent* TrackEventList::consume(int startFrame, int blockFrames)
{
    TrackEvent* found = nullptr;
    
    int maxFrame = startFrame + blockFrames - 1;
    TrackEvent* prev = nullptr;
    TrackEvent* e = events;
    while (e != nullptr) {
        if (!e->pending && e->frame >= startFrame && e->frame <= maxFrame) {
            found = e;
            break;
        }
        else {
            prev = e;
            e = e->next;
        }
    }
    if (found != nullptr) {
        if (prev == nullptr)
          events = found->next;
        else
          prev->next = found->next;
        found->next = nullptr;
    }
    return found;
}

TrackEvent* TrackEventList::consumePulsed()
{
    TrackEvent* found = nullptr;
    
    TrackEvent* prev = nullptr;
    TrackEvent* e = events;
    while (e != nullptr) {
        if (e->pulsed) {
            found = e;
            break;
        }
        else {
            prev = e;
            e = e->next;
        }
    }
    if (found != nullptr) {
        if (prev == nullptr)
          events = found->next;
        else
          prev->next = found->next;
        found->next = nullptr;
    }
    return found;
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
