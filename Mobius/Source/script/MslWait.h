/**
 * Model used by the MSL interpreter to ask the MslContext to schedule
 * a wait.
 */

#pragma once

#include "MslValue.h"

// ugh, usual namespace issues
// either prefix these or move them inside MslWait

/**
 * There are three fundamental wait types.
 *
 *   Event waits for something to happen at an unspecified time in the future
 *   Location waits for a specific point within the loop
 *   Duration waits for a specific period of time
 *
 */
typedef enum {
    WaitTypeNone,
    WaitTypeEvent,
    WaitTypeDuration,
    WaitTypeLocation
} MslWaitType;

/**
 * Event waits end when one of these events are detected at an
 * undefined time.
 */
typedef enum {
    WaitEventNone,
    // overlap with these and location events, consider whether we need both
    WaitEventLoop,  // start of the loop, frame zero after the transition
    WaitEventEnd,   // end of the loop, just before the transition
    
    WaitEventCycle,
    WaitEventSubcycle,
    WaitEventBeat,
    WaitEventBar,
    WaitEventMarker,
    WaitEventLast,
    WaitEventSwitch,
    WaitEventBlock,
    // from here down, they're iffy and may be not necessary
    // but the old scripts defined them
    WaitEventExternalStart,
    WaitEventPulse,
    WaitEventRealign,
    WaitEventReturn,
    WaitEventDriftCheck
} MslWaitEvent;

/**
 * Duration waits end after some number of sample frames advance.
 * The frame advance is calculated with a count applied to these units.
 *
 * todo: might be interesting to allow the specification of arbitrary
 * durations between two markers.
 */
typedef enum {
    WaitDurationNone,
    WaitDurationFrame,
    WaitDurationMsec,
    WaitDurationSecond,
    WaitDurationSubcycle,
    WaitDurationCycle,
    WaitDurationLoop,
    WaitDurationBeat,
    WaitDurationBar
} MslWaitDuration;

/**
 * Location waits end when one of these numbered locations within the loop
 * is reached.  You can accomplish the same thing by first using
 * an event "Wait loop" and then an event "Wait subcycle".  This is just
 * syntactic sugar to avoid two different wait statements and reads nicer.
 */
typedef enum {
    WaitLocationNone,
    // overlap with these and event waits, need both?
    // if we leave them here, they won't have meaningful nubmers
    WaitLocationStart,
    WaitLocationEnd,
    // these have numbers
    WaitLocationSubcycle,
    WaitLocationCycle,
    WaitLocationBeat,
    WaitLocationBar,
    // unclear about these, could have a default number in order of insertion
    // but they'll usually be referenced by name?
    WaitLocationMarker
} MslWaitLocation;

/**
 * The Wait object has those three enumerations plus information
 * about how to get back to the things that need to be notified when
 * the wait condition is reached.
 */
class MslWait
{
  public:

    MslWaitType type = WaitTypeNone;
    MslWaitEvent event = WaitEventNone;
    MslWaitDuration duration = WaitDurationNone;
    MslWaitLocation location = WaitLocationNone;

    // arguments from the Wait expression to pass to the MslContext
    // this is the duration, location, counter, etc.
    int arguments = 0;

    // true if this wait is active
    // since all MslStacks have an embedded MslWait this says whether
    // it has been turned on or is dormant
    bool active = false;

    // true once an active wait is over
    // this is relevant only if active is also true
    // it is set by the MslContext when something appropriate happens
    // or by MslEnvironment if it decides the wait has timed out
    bool finished = false;

    // Where the wait came from 

    // Unclear if we realy need the session/stack here
    // the outside world just needs to set the finished flag
    // when the wait is over, the session will then resume from
    // whatever frame contains this wait object

    // the session that is waitin
    class MslSession* session = nullptr;

    // the stack frame that is waiting
    class MslStack* stack = nullptr;

    // initialize runtime wait state when the containing MslStack
    // is brought out of the pool
    // the only import thing is the active flag, but I suppose
    // we could put it all back to rest to make it look better in the debugger
    void reset() {
        active = false;
    }
    
};
