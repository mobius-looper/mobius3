/**
 * Encapsulates code related to the core Mobius integration with MSL.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../script/MslContext.h"
#include "../../script/MslExternal.h"
#include "../../script/MslWait.h"
#include "../../script/MslValue.h"
#include "../../script/ScriptExternals.h"

// for MobiusContainer
#include "../MobiusInterface.h"

#include "Mobius.h"
#include "Track.h"
#include "Loop.h"
#include "Event.h"
#include "EventManager.h"

#include "MobiusMslHandler.h"

MobiusMslHandler::MobiusMslHandler(Mobius* m) //  : variables(m)
{
    mobius = m;
}

MobiusMslHandler::~MobiusMslHandler()
{
}

//////////////////////////////////////////////////////////////////////
//
// Query
//
//////////////////////////////////////////////////////////////////////

// should no longer be used
// MobiusLooperTrack will call various things on the Track
#if 0
/**
 * Handle an MSL query on an internal variable.
 * Symbol queries will have been handled by MobiusKernel.
 */
bool MobiusMslHandler::mslQuery(MslQuery* query)
{
    bool success = false;
    Track* track = nullptr;
    
    if (query->scope > 0) {
        int trackIndex = query->scope - 1;
        if (trackIndex < mobius->getTrackCount())
          track = mobius->getTrack(trackIndex);
    }

    if (track == nullptr) {
        Trace(1, "Mobius: MSL variable query with invalid track scope");
    }
    else {
        success = variables.get(query, track);
    }
    
    return success;
}
#endif

/**
 * This was the original implementation that used the old
 * ScriptInternalVariables.  Keep around for reference for awhile
 * in case this becomes necessary again.
 */
#if 0
bool MobiusMslHandler::mslQuery(MslQuery* query)
{
    bool success = false;
    ScriptExternalType type = (ScriptExternalType)(query->external->type);
    
    // symbols have been handled in an different path
    if (type == ExtTypeOldVariable) {
        ScriptInternalVariable* var = static_cast<ScriptInternalVariable*>(query->external->object);
        Track* track = mobius->getTrack();
        if (query->scope > 0) {
            track = mobius->getTrack(query->scope);
            if (track == nullptr)
              query->error.setError("Track number out of range");
        }
        if (track != nullptr) {

            // if this isn't a track variable, it will always return 0
            ExValue v;
            var->getTrackValue(track, &v);

            // ExValue is pretty much the same as MslValue
            // but we don't have a shared copyier for them
            // might want that

            success = true;
            if (v.getType() == EX_INT) {
                query->value.setInt(v.getInt());
            }
            else if (v.getType() == EX_FLOAT) {
                query->value.setFloat(v.getFloat());
            }
            else if (v.getType() == EX_BOOL) {
                query->value.setBool(v.getBool());
            }
            else if (v.getType() == EX_STRING) {
                query->value.setString(v.getString());
            }
            else {
                // must be EX_LIST, we don't support that in MslQuery yet
                // but there are no variables that need it
                query->error.setError("Queries returning lists not supported");
                success = false;
            }
        }
    }
    else {
        Trace(1, "Mobius: MQL Query request not for a Variable");
    }
    return success;
}
#endif

//////////////////////////////////////////////////////////////////////
//
// MSL Script Waits
//
//////////////////////////////////////////////////////////////////////

/**
 * The target track is supposed to be passed in the MslWait
 * if the script is using an "in" statement for track scoping.
 * I guess this can deafult to the active track since everything
 * else works that way.
 */
Track* MobiusMslHandler::getWaitTarget(MslWait* wait)
{
    // defaults to active
    Track* track = mobius->getTrack();

    if (wait->track > 0) {
        track = mobius->getTrack(wait->track);
        if (track == nullptr) {
            Trace(1, "Mobius: MslWait with invalid track number %d", wait->track);
            // fatal or default?
            track = mobius->getTrack();
        }
    }
    return track;
}

/**
 * All frame-based waits reduce to a single call to schedule an event
 * on tht frame.  The ame location/duration/repeats calculations can be
 * shared with Mobius core and MIDI tracks.
 */
bool MobiusMslHandler::scheduleWaitFrame(MslWait* wait, int frame)
{
    bool success = false;
    Track* track = getWaitTarget(wait);
    if (track != nullptr) {
        EventManager* em = track->getEventManager();
        Event* e = em->newEvent();
        
        e->type = ScriptEvent;
        e->frame = frame;

        // what old code did and we have no way to pass right now
        // need this for location waits too
#if 0            
        // special option to bring us out of pause mode
        // Should really only allow this for absolute millisecond waits?
        // If we're waiting on a cycle should wait for the loop to be
        // recorded and/or leave pause.  Still it could be useful
        // to wait for a loop-relative time.
        e->pauseEnabled = mInPause;

        // !! every relative UNIT_MSEC wait should be implicitly
        // enabled in pause mode.  No reason not to and it's what
        // people expect.  No one will remember "inPause"
        if (mWaitType == WAIT_RELATIVE && mUnit == UNIT_MSEC)
          e->pauseEnabled = true;
#endif            

        // old scripts set the ScriptInterpreter on the event
        // here we set the MslWait which triggers a parallel set of logic
        // everywhere a ScriptInterpreter would be found
        //e->setScript(si);
        e->setMslWait(wait);
        
        em->addEvent(e);

        // the old interpreter would set the event on the ScriptStack
        // here we save it in the Wait object itself
        // this actually isn't necessary MSL won't do anything with it,
        // it is only used to pass the previous async event IN when
        // setting up a "wait last"
        wait->coreEvent = e;

        // remember this while we're here, could be useful
        // we can trace it once we get back
        wait->coreEventFrame = frame;

        success = true;
    }
    return success;
}

/**
 * Schedule a wait on something that can't be calculated to a specific
 * frame
 */
bool MobiusMslHandler::scheduleWaitEvent(MslWait* wait)
{
    bool success = false;
    Track* track = getWaitTarget(wait);
    if (track != nullptr) {

        if (wait->type == MslWaitLast) {

            // this works quite differently than MOS scripts
            // if a previous action scheduled an event, that would have
            // been returned in the UIAction and MSL would have remembered it
            // when it reaches a "wait last" it passes that event back down
            // unlike MOS, we're only dealing with Events here not ThreadEvents
            Event* event = (Event*)wait->coreEvent;
            if (event == nullptr) {
                // should not have gotten this far
                wait->finished = true;
            }
            else {
                // don't assume this object is still valid
                // it almost always will be, but if there was any delay between
                // the last action and the wait it could be gone
                EventManager* em = track->getEventManager();
                if (!em->isEventScheduled(event)) {
                    // yep, it's gone, don't treat this as an error
                    wait->finished = true;
                }
                else {
                    // and now we wait
                    event->setMslWait(wait);
                    // set this while we're here though nothing uses it
                    wait->coreEventFrame = (int)(event->frame);
                }
            }
            success = true;
        }
        else {
            Trace(1, "MobiusMslHandler::scheduleWaitEvent Unexpected wait type");
        }
    }

    return success;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
