/* 
 * A model for track events and an event list.
 * Most events should be allocated and freed through EventManager.
 * A very few places (Synchronizer, MidiQueue, MidiTransport) may allocate
 * simple events to represent sync events.
 *
 * See EventManager for more on the relationship between events.
 *
 * A single EventPool is created by Mobius on startup and deleted
 * on shutdown.
 *
 * I really dislike the subtlety around processed/unprocessed children
 * and what free() actually does.  It is EXTREMELY confusing.
 */

#include <stdio.h>
#include <memory.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../model/TrackState.h"

#include "Action.h"
#include "Event.h"
#include "EventManager.h"
#include "Loop.h"
#include "Mobius.h"
#include "Synchronizer.h"
#include "Script.h"
#include "Track.h"
#include "Function.h"
#include "ScriptInterpreter.h"
#include "Mem.h"

//////////////////////////////////////////////////////////////////////
//
// Globals
//
//////////////////////////////////////////////////////////////////////

/**
 * Return a static string representation of a SyncPulseType value.
 * Intended for trace.
 */
const char* GetSyncPulseTypeName(SyncPulseType type)
{
    const char* name = "???";
    switch (type) {
        case SYNC_PULSE_UNDEFINED: name = "Undefined"; break;
        case SYNC_PULSE_CLOCK: name = "Clock"; break;
        case SYNC_PULSE_BEAT: name = "Beat"; break;
        case SYNC_PULSE_BAR: name = "Bar"; break;
        case SYNC_PULSE_SUBCYCLE: name = "Subcycle"; break;
        case SYNC_PULSE_CYCLE: name = "Cycle"; break;
        case SYNC_PULSE_LOOP: name = "Loop"; break;
    }
    return name;
}

//////////////////////////////////////////////////////////////////////
//
// Base EventType
//
//////////////////////////////////////////////////////////////////////

EventType::EventType()
{
	name = nullptr;
	displayName = nullptr;
	reschedules = false;
	noUndo = false;
	noMode = false;
}

const char* EventType::getDisplayName()
{
	return ((displayName != nullptr) ? displayName : name);
}

/**
 * By default we forward to the function's event handler.
 * This may also be overloaded in a subclass but it is rare.
 */
void EventType::invoke(Loop* l, Event* e)
{
	if (e->function == nullptr)
	  Trace(l, 1, "Cannot do event, no associated function!\n");
	else
	  e->function->doEvent(l, e);
}

/**
 * By default we forward to the function's undo handler.
 * This may also be overloaded in a subclass.
 */
void EventType::undo(Loop* l, Event* e)
{
	if (e->function == nullptr)
	  Trace(l, 1, "Cannot undo event, no associated function!\n");
	else
	  e->function->undoEvent(l, e);
}

/**
 * By default we forward to the function's confirm handler.
 * This may also be overloaded in a subclass.
 */
void EventType::confirm(Action* action, Loop* l, Event* e, long frame)
{
    if (e->function == nullptr)
	  Trace(l, 1, "Cannot confrim event, no associated function!\n");
	else
	  e->function->confirmEvent(action, l, e, frame);
}

/**
 * Default move event handler.
 * This was originally here for a speed recaluation which it turns
 * out we can defer, so we don't really need this abstraction.
 */
void EventType::move(Loop* l, Event* e, long newFrame)
{
    EventManager* em = l->getTrack()->getEventManager();
    em->moveEvent(l, e, newFrame);
}

/****************************************************************************
 *                                                                          *
 *                                 EVENT POOL                               *
 *                                                                          *
 ****************************************************************************/

EventPool::EventPool()
{
    mEvents = NEW(EventList);
    mAllocated = 0;
}

/**
 * This is subtle because we use an EventList to maintain the pooled list
 * but ~EventList wants to return the Events it contains to the pool.
 * So to prevent leaks and an infinte loop we "steal" the list from
 * the EventList and do a cascade delete on the next pointer.
 * See notes/event-list.txt
 */
EventPool::~EventPool()
{
    Trace(2, "EventPool: Destructing\n");
    
    Event* list = mEvents->steal();
    Event* next = nullptr;
    
    while (list != nullptr) {
        next = list->getNext();
        // actually don't have to do this since ~Event doesn't cascade
        // but follow the usual pattern
        list->setNext(nullptr);
        delete list;
        list = next;
    }

    // now it is empty and won't try to flush() back to the pool
    delete mEvents;
}

/**
 * Allocate an event from the pool.
 */
Event* EventPool::newEvent()
{
    Event* e = nullptr;

    if (mEvents != nullptr) {
        e = mEvents->getEvents();
        if (e != nullptr) {
            mEvents->remove(e);
            e->init();
            e->setPooled(false);
        }
    }

    if (e == nullptr) {
        e = NEW1(Event, this);
        mAllocated++;
        // Trace(2, "Allocated event at %10x\n", (int)e);
    }

    return e;
}

/**
 * The core event freeer.
 *
 * Ignore if the event has a parent, the event will be freed later
 * when the parent is freed.  If there are any processed children
 * free them also.  If there are unprocessed children, leave them
 * alone unless the "freeAll" flag is set, they may still be scheduled.
 * I'm not sure this can happen, it's probably an error in event timing?
 */
void EventPool::freeEvent(Event* e, bool freeAll)
{
	// ignore if we have a parent, or are "owned"
	if (e != nullptr && e->getParent() == nullptr && !e->isOwned()) {

		if (e->isPooled()) {
			// shouldn't happen if we're managing correctly
			Trace(1, "Freeing event already in the pool!\n");
		}
		else {
			// Just to be safe, let the script interpreter know in case
			// it is still waiting on this.  Shouldn't happen if
			// we're processing events properly.
			ScriptInterpreter* script = e->getScriptInterpreter();
			if (script != nullptr) {
				// return strue if we were actually waiting on this
				if (script->cancelEvent(e))
				  Trace(1, "Attempt to free an event a script is waiting on!\n");
				e->setScriptInterpreter(nullptr);
			}
            // parallel shit for MSL, fuck we don't have a Mobius here
            // this isn't supposed to happen, just let it hang I guess
            // !! need to be notifying the MslEnvironment that this is being canceled
            class MslWait* wait = e->getMslWait();
            if (wait != nullptr) {
                Trace(1, "EventPool: Freeing event with lingering MslWait!!");
            }

			// if we have children, set them free
			Event* next = nullptr;
			for (Event* child = e->getChildren() ; child != nullptr ; child = next) {
				next = child->getSibling();

				// NOTE: In a few special cases for shared events,
				// we may have something on our child list we don't own
				if (child->getParent() == e) {

					if (freeAll || child->processed) {
						child->setParent(nullptr);
						freeEvent(child, freeAll);
					}
					else {
						Trace(1, "Freeing event with unprocessed children! %s/%s\n",
							  e->type->name, child->type->name);
						child->setParent(nullptr);
					}
				}
			}

            // !! normally have a csect around list manipulations
			if (e->getList() != nullptr) {
                Trace(1, "Freeing event still on a list!\n");
                e->getList()->remove(e);
            }

            // Should not still have an Action, if we do it is usually
            // an ownership error, be safe and let it leak 
            Action* action = e->getAction();
            if (action != nullptr) {
                Trace(1, "Event::freeEvent leaking Action!\n");
                if (action->getEvent() == e)
                  action->detachEvent(e);
                e->setAction(nullptr);
            }

            if (e->getPool() == nullptr) {
                Trace(1, "Return unpooled event to a pool!\n");
            }

			mEvents->add(e);
		}
	}
}

/**
 * Static method to free a list of events chained on the next pointer.
 * This was added for Synchronizer and sync events, be careful because
 * this isn't always applicable to other event lists.
 */
#if 0
void EventPool::freeEventList(Event* event)
{
    while (event != nullptr) {
        Event* next = event->getNext();

        // second arg is freeAll
        freeEvent(event, true);

        event = next;
    }
}
#endif

void EventPool::dump()
{
    int pooled = 0;
    if (mEvents != nullptr) {
        for (Event* head = mEvents->getEvents() ; head != nullptr ; 
             head = head->getNext())
          pooled++;
    }

    Trace(2, "EventPool: %d allocated, %d in the pool, %d in use\n",
          mAllocated, pooled, mAllocated - pooled);
}

/****************************************************************************
 *                                                                          *
 *                                   EVENT                                  *
 *                                                                          *
 ****************************************************************************/

Event::Event(EventPool* pool)
{
	init();

    mPool = pool;
    mPooled = false;

	// this is permanent
	mOwned = false;
}

void Event::init()
{
	processed	= false;
	reschedule	= false;
	pending		= false;
	immediate	= false;
	type		= RecordEvent;
	function  	= nullptr;
	number		= 0;
	down		= true;
	longPress	= false;
	frame		= 0;
	latencyLoss = 0;
	quantized 	= false;
	afterLoop 	= false;
	pauseEnabled = false;
	automatic	= false;
	insane		= false;
    fadeOverride = false;
    silent      = false;

	mOwned		= false;
    mList       = nullptr;
	mNext		= nullptr;
	mParent		= nullptr;
	mChildren	= nullptr;
	mSibling	= nullptr;
	mScript     = nullptr;
    mMslWait    = nullptr;
    mAction     = nullptr;
	mInvokingFunction = nullptr;
    mArguments  = nullptr;

    mInfo[0] = 0;

    int fsize = sizeof(fields);
    memset(&fields, 0, fsize);
}

void Event::init(EventType* etype, long eframe)
{
	init();
	type = etype;
	frame = eframe;
}

/**
 * This should only be called by EventPool.
 * Most system code should use one of the free methods.
 */
Event::~Event()
{
}

/**
 * Free this event and the processed children, but leave the unprocessed
 * children.
 */
void Event::free()
{
    if (mPool != nullptr) {
        // I want to know if an event can be marked own yet still be from a pool
        // if this never happens then we don't need both states, lack of pool
        // implies ownership.  This won't actually pool it since
        // EventPool::freeEvent(bool, bool) also tests ownership
        if (mOwned)
          Trace(1, "Event::free Owned event being returned to the pool\n");
        mPool->freeEvent(this, false);
    }
    else if (!mOwned) {
        // this happens in new code for the two cases I could find that want
        // to make an owned event outside of a pool Synchronizer::mReturnEvent
        // and EventManager::mSyncEvent
        // they now won't have a pool, but they should be marked owned
        // made it less confusing than getting them from a pool only to suppress
        // returning them to it with the ownership flag.
        // should just have lack of pool imply ownership and not mess with
        // even attempting it above
        // if owned is not set, then we have something that is both not owned
        // and not in a pool which shouldn't happen
        Trace(1, "Event::free no pool and not owned!\n");
    }
}

/**
 * Free this event and all children even if not processed.
 */
void Event::freeAll()
{
    if (mPool != nullptr) {
        mPool->freeEvent(this, true);
    }
    else if (!mOwned) {
        Trace(1, "Event::freeAll no pool!\n");
    }
}

void Event::setPooled(bool b)
{
    mPooled = b;
}

bool Event::isPooled()
{
    return mPooled;
}

void Event::setOwned(bool b)
{
    mOwned = b;
}

bool Event::isOwned()
{
    return mOwned;
}

void Event::setList(EventList* list)
{
    mList = list;
}

EventList* Event::getList()
{
    return mList;
}

void Event::setNext(Event* e) 
{
    mNext = e;
}

Event* Event::getNext()
{
    return mNext;
}

void Event::setSibling(Event* e) 
{
    mSibling = e;
}

Event* Event::getSibling()
{
    return mSibling;
}

void Event::setParent(Event* parent) 
{
    mParent = parent;
}

Event* Event::getParent()
{
    return mParent;
}

Event* Event::getChildren()
{
    return mChildren;
}

Track* Event::getTrack() 
{
	return mTrack;
}

void Event::setTrack(Track* t) 
{
	mTrack = t;
}

/**
 * The interpreter that scheduled the event.
 */
ScriptInterpreter* Event::getScriptInterpreter() 
{
	return mScript;
}

void Event::setScriptInterpreter(ScriptInterpreter* si) 
{
	mScript = si;
}

class MslWait* Event::getMslWait()
{
    return mMslWait;
}

void Event::setMslWait(class MslWait* w)
{
    mMslWait = w;
}

/**
 * The script arguments.
 */
ExValueList* Event::getArguments()
{
    return mArguments;
}

/**
 * Release the script arguments.
 * This isn't done automatically, why!?
 * Intead of having every function do this just do it when we return
 * the event to the pool.
 */
void Event::clearArguments()
{
    delete mArguments;
    mArguments = nullptr;
}

void Event::setArguments(ExValueList* args)
{
    // shouldn't see this?
    if (mArguments != nullptr) {
        Trace(1, "Replacing arguments in event");
        delete mArguments;
    }
    mArguments = args;
}

void Event::setAction(Action* a)
{
    // this is probably okay but I want to start removing this
    // yes, it happens with Action::changeEvent
    if (mAction == nullptr && mInvokingFunction != nullptr) 
      Trace(2, "Event::setAction already had an invoking function\n");

    if (a != nullptr && mInvokingFunction != nullptr && 
        mInvokingFunction != a->getFunction())
      Trace(1, "Event::setAction mismatched action/invoking function\n");

    mAction = a;
}

Action* Event::getAction()
{
    return mAction;
}

void Event::setInvokingFunction(Function* f)
{
    mInvokingFunction = f;
    if (mAction != nullptr && mAction->getFunction() != f) {
        // I don't think this should allowed
        Trace(1, "Event::setInvokingFunction mismatched action/invoking function\n");
    }
    else if (mAction == nullptr) {
        // this is okay, but I want to start weeding them out
        Trace(2, "Event::setInvokingFunction without action\n");
    }
}

/**
 * Continue supporting an explicitly set function, but
 * fall back to the Action if we have one.
 */
Function* Event::getInvokingFunction()
{
    Function* f = mInvokingFunction;
    if (f == nullptr && mAction != nullptr)
      f = mAction->getFunction();
    return f;
}

const char* Event::getName()
{
	return type->name;	
}

const char* Event::getFunctionName()
{
	// since this is only used for trace, always return a non-null value
	return (function != nullptr) ? function->getName() : "";
}

const char* Event::getInfo()
{
    return (mInfo[0] != 0) ? mInfo : nullptr;
}

void Event::setInfo(const char* src)
{
    if (src == nullptr)
      mInfo[0] = 0;
    else
      CopyString(src, mInfo, sizeof(mInfo));
}

/**
 * Add a child event to the end of the list.
 */
void Event::addChild(Event* e)
{
	// this is now happening when we stack events under a SwitchEvent
	// probably not necessary but make them consistent
	if (pending && !e->pending)
	  e->pending = true;

	if (e != nullptr) {
		// order these for undo and display
		Event* last = nullptr;
		for (last = mChildren ; last != nullptr && last->mSibling != nullptr ; 
			 last = last->mSibling);
		if (last == nullptr)
		  mChildren = e;
		else
		  last->mSibling = e;

		e->mParent = this;
	}
}

/**
 * Remove a child event. The event is not freed.
 */
void Event::removeChild(Event* event)
{
	Event* prev = nullptr;
	Event* found = nullptr;

	for (Event* e = mChildren ; e != nullptr ; e = e->mSibling) {
		if (e != event)
		  prev = e;
		else {
			found = e;
			break;
		}
	}

	if (found != nullptr) {
		if (prev == nullptr)
		  mChildren = found->mSibling;
		else
		  prev->mSibling = found->mSibling;

		found->mSibling = nullptr;
		found->mParent = nullptr;
	}
	else {
	  Trace(1, "Expected child event not found\n");
    }
}

/**
 * Remove the last child event that isn't a JumpPlayEvent.
 * This is used when undoing events stacked for application 
 * after a loop switch.  JumpPlayEvent cannot be undone 
 * until the parent SwitchEvent is undone.
 *
 * The event is not freed, and no undo semantics happen.
 */
Event* Event::removeUndoChild()
{
	Event* undo = nullptr;

	for (Event* e = mChildren ; e != nullptr ; e = e->mSibling) {
		if (e->type != JumpPlayEvent)
		  undo = e;
	}

	if (undo != nullptr)
	  removeChild(undo);

	return undo;
}

/**
 * Search the child event list for one of a given type.
 */
Event* Event::findEvent(EventType* childtype)
{
	Event* found = nullptr;
	for (Event* e = mChildren ; e != nullptr ; e = e->mSibling) {
		if (e->type == childtype) {
			found = e;
			break;
		}
	}
	return found;
}

/**
 * Search the child event list for an event of a given type and function.
 * In practice used only for finding InvokeEvents.
 */
Event* Event::findEvent(EventType* childtype, Function* f)
{
	Event* found = nullptr;
	for (Event* e = mChildren ; e != nullptr ; e = e->mSibling) {
		if (e->type == childtype && e->function == f) {
			found = e;
			break;
		}
	}
	return found;
}

/**
 * Return true if any of our child events have already been processed.
 */
bool Event::inProgress()
{
	bool started = false;
	
	for (Event* e = mChildren ; e != nullptr ; e = e->mSibling) {
		if (e->processed) {
			started = true;
			break;
		}
	}

	if (started) {
		// should also not be in these states
		if (pending)
		  Trace(1, "Pending event considered in progress!\n");

		if (reschedule)
		  Trace(1, "Reschedulable event considered in progress!\n");
	}

	return started;
}

/****************************************************************************
 *                                                                          *
 *                              EVENT PROCESSING                            *
 *                                                                          *
 ****************************************************************************/

/**
 * Execute an event. 
 * Have to redirect through the EventType since not all events
 * will be associated with Functions.
 */
void Event::invoke(Loop* l)
{
    if (type != nullptr) {
        // some types may overload this
        type->invoke(l, this);
    }
    else if (function != nullptr) {
        // appeared in a few cases in 1.45 but shouldn't happen now?
        Trace(1, "Event without type!\n");
        function->doEvent(l, this);
    }
    else {
        Trace(1, "No way to invoke event!\n");
    }
}

/**
 * Undo an event. 
 * Have to redirect through the EventType since not all events
 * will be associated with Functions.
 */
void Event::undo(Loop* l)
{
	type->undo(l, this);
}

/**
 * Confirm the event on the given frame.  If frame is EVENT_FRAME_IMMEDIATE (-1)
 * the event is expected to be scheduled immediately in the target loop.
 * If the event frame is EVENT_FRAME_CALCULATED (-2) the event handler
 * is allowed to calculate the frame, though usually this will behave 
 * the same as IMMEDIATE.
 *
 * If the frame is positive the event is activated for that frame.
 */
void Event::confirm(Action* action, Loop* l, long argFrame)
{
    type->confirm(action, l, this, argFrame);
}

/**
 * Tell the interpreter the event has finished.
 * This will cause the script to be resumed after the wait the
 * next time it runs.
 *
 * !! NO it runs synchronously.  I don't like that at all...
 *
 * new: had to start passing Mobius in to deal with MslWait
 */
void Event::finishScriptWait(Mobius* m) 
{
	if (mScript != nullptr)
	  mScript->finishEvent(this);

    if (mMslWait != nullptr)
      m->finishMslWait(this);
}

/**
 * Tell the interpreter the event has been rescheduled.
 * If the interpreter was waiting on this event, then it can
 * switch to waiting on the new event.
 */
void Event::rescheduleScriptWait(Mobius* m, Event* neu) 
{
	if (mScript != nullptr)
	  mScript->rescheduleEvent(this, neu);

    if (mMslWait != nullptr)
      m->rescheduleMslWait(this, neu);
}

/**
 * If this event is being monitored by a ScriptInterpreter, let it
 * know that the event is being caneled.  
 *
 * !! I'm not sure if this is working.  This was being called by
 * accident from Synchronier::doRealign on the Realign event and the
 * script wasn't waking up!
 *
 */
void Event::cancelScriptWait(Mobius* m)
{
    if (mScript != nullptr) {
		mScript->cancelEvent(this);
        mScript = nullptr;
	}
    
    if (mMslWait != nullptr) {
        m->cancelMslWait(this);
        mMslWait = nullptr;
	}

    // new style TrackWait support, similar to MslWait
    if (fields.loopSwitch.waiting)
      m->cancelTrackWait(this);
    
}

/****************************************************************************
 *                                                                          *
 *                                 EVENT LIST                               *
 *                                                                          *
 ****************************************************************************/

EventList::EventList()
{
    mEvents = nullptr;
}

EventList::~EventList()
{
    flush(true, false);
}

/**
 * If the "reset" flag is on, then we flush everything.  If the flag
 * is off then we only flush "undoable" events.  The reset flag will
 * be off in cases where we're making a transition that invalidates
 * major scheduled events, but needs to keep play jumps and other
 * invisible housekeeping events.
 *
 * If the keepScriptEvents flag is on, we will retain script wait events
 * when resetting.  This is normally off, and should only be on if we 
 * know we're running a script that is waiting for a Reset, or otherwise
 * expected to be running after the Reset.  
 *
 */
void EventList::flush(bool reset, bool keepScriptEvents)
{
	Event* next = nullptr;
	for (Event* e = mEvents ; e != nullptr ; e = next) {
		next = e->getNext();
		if (reset || !e->type->noUndo) {
			if (e->type != ScriptEvent || (reset && !keepScriptEvents)) {

				remove(e);
				// remove doesn't free but free can remove children
				// which may be the next on the list, so we have to 
				// start over from the beginning after any free
				// if we're resetting turn on the freeAll flag
                if (reset)
                  e->freeAll();
                else
                  e->free();
				next = mEvents;
			}
		}
	}
}

/**
 * Specialty function for loop switch to transfer
 * all of the current events to a new list.
 */
EventList* EventList::transfer()
{
	EventList* list = NEW(EventList);

	for (Event* e = mEvents ; e != nullptr ; e = e->getNext())
	  e->setList(list);

	list->mEvents = mEvents;
	mEvents = nullptr;

	return list;
}

Event* EventList::getEvents()
{
	return mEvents;
}

/**
 * Add an event to the end of the list.
 * !! name this append to make it clear how this is different
 * than insert()
 */
void EventList::add(Event* event)
{
	if (event != nullptr) {

		if (event->getList() != nullptr) {
            Trace(1, "Attempt to add an event already on another list!\n");
        }
		else {
			Event* last = nullptr;
			for (last = mEvents ; last != nullptr && last->getNext() != nullptr ; 
				 last = last->getNext());

			if (last != nullptr)
			  last->setNext(event);
			else
			  mEvents = event;

			event->setList(this);
		}
	}
}

/**
 * Insert an event into the list, ordering by frame.
 */
void EventList::insert(Event* event)
{
	if (event != nullptr) {

		if (event->getList() != nullptr) {
            Trace(1, "Attempt to add an event already on another list!\n");
        }
		else {
            Event* prev = nullptr;
            for (Event* e = mEvents ; e != nullptr ; e = e->getNext()) {
                if (e->frame > event->frame)
                  break;
                else 
                  prev = e;
            }   

            if (prev != nullptr) {
                event->setNext(prev->getNext());
                prev->setNext(event);
            }
            else {
                event->setNext(mEvents);
                mEvents = event;
            }

            event->setList(this);
        }
    }
}
                
/**
 * Remove an event from the list.
 * The event is not freed, it is simply removed.
 */
void EventList::remove(Event* event)
{
	if (event != nullptr) {
		Event* prev = nullptr;
		Event* e;

		for (e = mEvents ; e != nullptr && e != event ; e = e->getNext())
		  prev = e;

		if (e == event) {
			if (prev == nullptr)
			  mEvents = e->getNext();
			else 
			  prev->setNext(e->getNext());

			e->setList(nullptr);
			e->setNext(nullptr);
		}
	}
}

/**
 * Return true if the event is in the list.
 */
bool EventList::contains(Event* event)
{
	bool contains = false;

	for (Event* e = mEvents ; e != nullptr ; e = e->getNext()) {
		if (e == event) {
			contains = true;
			break;
		}
	}
	return contains;
}

/**
 * Return the first event on a frame.
 */
Event* EventList::find(long frame)
{
	Event* event = nullptr;
	for (Event* e = mEvents ; e != nullptr ; e = e->getNext()) {
		if (e->frame == frame) {
			event = e;
			break;
		}
	}
	return event;	
}

/**
 * Return the next event of a given type.
 */
Event* EventList::find(EventType* type)
{
	Event* event = nullptr;
	for (Event* e = mEvents ; e != nullptr ; e = e->getNext()) {
		if (e->type == type) {
			event = e;
			break;
		}
	}
	return event;	
}

Event* EventList::find(Function* f)
{
	Event* event = nullptr;
	for (Event* e = mEvents ; e != nullptr ; e = e->getNext()) {
		if (e->function == f) {
			event = e;
			break;
		}
	}
	return event;	
}

/**
 * Return an event of the given type on the given frame.
 */
Event* EventList::find(EventType* type, long frame)
{
	Event* event = nullptr;
	for (Event* e = mEvents ; e != nullptr ; e = e->getNext()) {
		if (e->type == type && e->frame == frame) {
			event = e;
			break;
		}
	}
	return event;	
}

/**
 * Reutrn the chain of events in this list and forget about them.
 * This ONLY for use by EventPool when it wants to delete the events
 * in the pool during shutdown.
 *
 * Since ~EventList returns the events it contains to the pool we get
 * into an infinite loop when EventPool tries to delete the EventList
 * that it is using to maintain the pooled list.
 *
 * See notes/event-list.txt for gory details on this awkward situation.
 * It is assumed that the events returned are chained with the getNext pointer.
 */
Event* EventList::steal()
{
    Event* stolen = mEvents;
    mEvents = nullptr;
    return stolen;
}

//////////////////////////////////////////////////////////////////////
//
// FollowerEvent
//
//////////////////////////////////////////////////////////////////////

/**
 * This is a special event type not associated with a Function or script.
 * They are scheduled at quantization points by a MIDI track follower
 * that needs to be informed when those quantization points are crossed.
 */
class FollowerEventType : public EventType {
  public:
    virtual ~FollowerEventType() {}
	FollowerEventType();
	void invoke(Loop* l, Event* e);
};

FollowerEventType::FollowerEventType()
{
	name = "Follower";
    stateEventType = TrackState::EventFollower;
}

void FollowerEventType::invoke(Loop* l, Event* e)
{
    l->getMobius()->followerEvent(l, e);
}

FollowerEventType FollowerEventObj;
EventType* FollowerEvent = &FollowerEventObj;

/**
 * Newer generalization of FoloowerEventType
 * Eventual replacement
 */
class WaitEventType : public EventType {
  public:
    virtual ~WaitEventType() {}
	WaitEventType();
	void invoke(Loop* l, Event* e);
};

WaitEventType::WaitEventType()
{
	name = "Wait";
    stateEventType = TrackState::EventWait;
}

void WaitEventType::invoke(Loop* l, Event* e)
{
    l->getMobius()->waitEvent(l, e);
}

WaitEventType WaitEventObj;
EventType* WaitEvent = &WaitEventObj;

/**
 * Newer event type to track the passing of cycles when
 * doing a synchronized recording with a free start.
 */
class SyncCycleEventType : public EventType {
  public:
    virtual ~SyncCycleEventType() {}
	SyncCycleEventType();
	void invoke(Loop* l, Event* e);
};

SyncCycleEventType::SyncCycleEventType()
{
	name = "SyncCycle";

    // !! hey, shouldn't we be setting these flags for WaitEventType too?
    noUndo = true;
    noMode = true;
    // don't need a type for this since it won't be shown
    //stateEventType = TrackState::EventSyncCycle;
}

void SyncCycleEventType::invoke(Loop* l, Event* e)
{
    l->getSynchronizer()->syncCycleEvent(l, e);
}

SyncCycleEventType SyncCycleEventObj;
EventType* SyncCycleEvent = &SyncCycleEventObj;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
