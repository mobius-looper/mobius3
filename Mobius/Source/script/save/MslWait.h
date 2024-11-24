/**
 * Model used by the MSL interpreter to ask the MslContext to schedule
 * a wait.
 */

#pragma once

#include "MslValue.h"

// ugh, usual namespace issues
// either prefix these or move them inside MslWait

/**
 * There are all the wait types.  They correspond to keywords that must
 * come after the wait statement keyword.
 */
typedef enum {
    WaitTypeNone,

    WaitSubcycle,
    WaitCycle,
    WaitLoop,
    WaitStart,      // symonym of WaitLoop
    WaitEnd,        // special meaning just before the loop point
    WaitBeat,
    WaitBar,
    WaitMarker,

    WaitFrame,
    WaitMsec,
    WaitSecond,
    WaitBlock,
    
    WaitLast,
    WaitSwitch,
    // from here down, they're iffy and may be not necessary
    // but the old scripts defined them
    WaitExternalStart,
    WaitPulse,
    WaitRealign,
    WaitReturn,
    WaitDriftCheck
    
} MslWaitType;

/**
 * The Wait object has those three enumerations plus information
 * about how to get back to the things that need to be notified when
 * the wait condition is reached.
 */
class MslWait
{
  public:

    //
    // Request State
    // This is what is passed down to the engine to schedule the wait
    //

    MslWaitType type = WaitTypeNone;

    // the numeric required amount of a few wait types: Frame, Msec, Second
    int amount = 0;

    // the number of repetitions
    int repeats = 0;

    // the location number
    int number = 0;

    // if the "next" keyword was found
    bool forceNext = false;

    //
    // Runtime
    //

    // the track this wait should be in, zero means active track
    int track = 0;

    //
    // Result State
    // This is what the engine passes back up after scheduling
    //

    // handle to an internal object that represents the wait event
    // for Mobius this is a core Event object
    void* coreEvent = nullptr;

    // loop frame on which the event was scheduled
    int coreEventFrame = 0;

    // flag that may be set on completion if the event was canceled
    // rather than being reached normally
    bool coreEventCanceled = false;

    //
    // Interpreter State
    // This is what the interpreter uses to track the status of the wait
    //

    // true if this wait is active
    // since all MslStacks have an embedded MslWait this says whether
    // it has been turned on or is dormant
    bool active = false;

    // true once an active wait is over
    // this is relevant only if active is also true
    // the context does not set this, the completion of a is performed
    // by calling MslEnvironment::resume
    bool finished = false;

    //
    // Where the wait came from
    // 
    
    // the session that is waiting
    class MslSession* session = nullptr;

    // the stack frame that is waiting
    // not necessary until sessions can have multiple execution threads
    class MslStack* stack = nullptr;
    
    /**
     * Initialize runtime wait state when the containing MslStack
     * is brought out of the pool.
     * The only important thing is the active flag, but it looks better in the
     * debugger to initialize all state.
     */
    void init() {
        type = MslTypeNone;
        amount = 0;
        repeats = 0;
        number = 0;
        forceNext = false;
        active = false;
        finished = false;
        track = 0;
        coreEvent = nullptr;
        coreEventFrame = 0;
        coreEventCanceled = false;
        session = nullptr;
        stack = nullptr;
    }
};