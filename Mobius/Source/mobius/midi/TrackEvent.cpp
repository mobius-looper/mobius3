

#include "../../util/Trace.h"

#include "../../model/ObjectPool.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"

#include "TrackEvent.h"

//////////////////////////////////////////////////////////////////////
//
// TrackEvent
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
void TrackEvent::poolInit()
{
    next = nullptr;
    type = EventNone;
    frame = 0;
    pending = false;
    pulsed = false;
    extension = false;
    primary = nullptr;
    stacked = nullptr;
    
    multiples = 0;
    switchTarget = 0;
    isReturn = false;
}

void TrackEvent::stack(UIAction* a)
{
    if (a != nullptr) {
        UIAction* prev = nullptr;
        UIAction* action = stacked;
        while (action != nullptr) {
            prev = action;
            action = action->next;
        }
        if (prev == nullptr)
          stacked = a;
        else
          prev->next = a;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Pool
//
//////////////////////////////////////////////////////////////////////

TrackEventPool::TrackEventPool()
{
    setName("TrackEvent");
    setObjectSize(sizeof(TrackEvent));
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
    clear();
}

void TrackEventList::initialize(TrackEventPool* p)
{
    pool = p;
}

void TrackEventList::clear()
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

TrackEvent* TrackEventList::find(TrackEvent::Type type)
{
    TrackEvent* found = nullptr;
    for (TrackEvent* e = events ; e != nullptr ; e = e->next) {
        if (e->type == type) {
            found = e;
            break;
        }
    }
    return found;
}

TrackEvent* TrackEventList::remove(TrackEvent::Type type)
{
    TrackEvent* found = nullptr;
    TrackEvent* prev = nullptr;
    
    for (TrackEvent* e = events ; e != nullptr ; e = e->next) {
        if (e->type == type) {
            found = e;
            break;
        }
        prev = e;
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

void TrackEventList::remove(TrackEvent* event)
{
    TrackEvent* found = nullptr;
    TrackEvent* prev = nullptr;
    
    for (TrackEvent* e = events ; e != nullptr ; e = e->next) {
        if (e == event) {
            found = e;
            break;
        }
        prev = e;
    }

    if (found != nullptr) {
        if (prev == nullptr)
          events = found->next;
        else
          prev->next = found->next;
        found->next = nullptr;
    }
}

TrackEvent* TrackEventList::findLast(SymbolId sym)
{
    TrackEvent* found = nullptr;
    for (TrackEvent* e = events ; e != nullptr ; e = e->next) {
        if (e->type == TrackEvent::EventAction &&
            e->primary != nullptr &&
            e->primary->symbol->id == sym) {
            found = e;
        }
    }
    return found;
}

TrackEvent* TrackEventList::consume(int startFrame, int endFrame)
{
    TrackEvent* found = nullptr;
    
    TrackEvent* prev = nullptr;
    TrackEvent* e = events;
    while (e != nullptr) {
        if (!e->pending && e->frame >= startFrame && e->frame <= endFrame) {
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

/**
 * Shift any events that are not pending down by some amount.
 * This is used in the case of events scheduled AFTER the end of the loop,
 * normally just loopFrames or 1+ the maxFrame calculated by consume()
 * I don't think we want consume() to take 1 beyond the block size because this
 * would pull ordinary events in a block early, but then without a shift the
 * end of loop event will never be reached.  Forget what audio loops do.
 *
 * Really don't like this.  The loop could in theory grow or shrink while
 * this is scheduled, so if that happens the previous event frame needs
 * to be adjusted 
 */
void TrackEventList::shift(int delta)
{
    for (TrackEvent* e = events ; e != nullptr ; e = e->next) {
        if (!e->pending && !e->pulsed) {
            // only shift events that are beyond the loop frame
            // it isn't obvious but "delta" is loopFrames
            if (e->frame >= delta) {
                int newFrame = e->frame - delta;
                Trace(2, "TrackEventList: Shifting event from %d to %d",
                      e->frame, newFrame);
                e->frame = newFrame;
            }
        }
    }
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

//////////////////////////////////////////////////////////////////////
//
// Utilieies
//
//////////////////////////////////////////////////////////////////////

/**
 * Relatively general utility to calculate quantization boundaries.
 * Adapted from core/EventManager.cpp
 *
 * TODO: Here is where we need to add some form of "gravity" to bring
 * quantization back to the previous boundary rather than the next one
 * if we're within a few milliseonds of the previous one.
 * new: this is an old note and I never did this.  It can be hard in the
 * general case since you have to go back in time and unwind things that
 * may have already been done.  No one compalined about it, so it sits
 * on a shelf.
 *
 * If "after" is false, we'll return the current frame if it is already
 * on a quantization boundary, otherwise we advance to the next one.
 * 
 * Subcycle quant is harder because the Subcycles divisor can result
 * in a roundoff error where subCycleFrames * subcycles != cycleFrames
 * Example:
 *
 *     cycleFrames = 10000
 *     subcycles = 7
 *     subCycleFrames = 1428.57, rounds to 1428
 *     cycleFrames * subcycles = 1428 * 7 = 9996
 * 
 * So when quantizing after the last subcycle, we won't get to the 
 * true end of the cycle.  Rather than the usual arithmetic, we just
 * special case when subCycle == subcycles.  This will mean that the
 * last subcycle will be slightly longer than the others but I don't
 * think this should be audible.  
 *
 * But note that for loops with many cycles, this calculation needs to be
 * performed within each cycle, rather than leaving it for the last
 * subcycle in the loop.  Leaving it to the last subcycle would result
 * in a multiplication of the roundoff error which could be quite audible,
 * and furthermore would result in noticeable shifting of the 
 * later subcycles.
 * 
 * This isn't a problem for cycle quant since a loop is always by definition
 * an even multiply of the cycle frames.
 * 
 */
int TrackEvent::getQuantizedFrame(int loopFrames, int cycleFrames, int currentFrame,
                                  int subcycles, QuantizeMode q, bool after)
{
    int qframe = currentFrame;

	// if loopFrames is zero, then we haven't ended the record yet
	// so there is no quantization
	if (loopFrames > 0) {

		switch (q) {
			case QUANTIZE_CYCLE: {
				int cycle = (int)(currentFrame / cycleFrames);
				if (after || ((cycle * cycleFrames) != currentFrame))


				  qframe = (cycle + 1) * cycleFrames;
			}
			break;

			case QUANTIZE_SUBCYCLE: {
				// this is harder due to float roudning
				// all subcycles except the last are the same size,
				// the last may need to be adjusted so that the combination
				// of all subcycles is equal to the cycle size

				// sanity check to avoid divide by zero
				if (subcycles == 0) subcycles = 1;
				int subcycleFrames = cycleFrames / subcycles;

				// determine which cycle we're in
				int cycle = (int)(currentFrame / cycleFrames);
				int cycleBase = cycle * cycleFrames;
				
				// now calculate which subcycle we're in
				int relativeFrame = currentFrame - cycleBase;
				int subcycle = (int)(relativeFrame / subcycleFrames);
				int subcycleBase = subcycle * subcycleFrames;

				if (after || (subcycleBase != relativeFrame)) {
					int nextSubcycle = subcycle + 1;
					if (nextSubcycle < subcycles)
					  qframe = nextSubcycle * subcycleFrames;
					else {
						// special case wrap to true end of cycle
						qframe = cycleFrames;
					}
					// we just did a relative quant, now restore the base
					qframe += cycleBase;
				}
			}
			break;

			case QUANTIZE_LOOP: {
				int loopCount = (int)(currentFrame / loopFrames);
				if (after || ((loopCount * loopFrames) != currentFrame))
				  qframe = (loopCount + 1) * loopFrames;
			}
			break;
			
			case QUANTIZE_OFF: {
			}
			break;
		}
	}

    return qframe;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
