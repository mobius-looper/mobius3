
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../script/MslContext.h"
#include "../../script/MslExternal.h"
#include "../../script/MslWait.h"
#include "../../script/MslValue.h"
#include "../../script/ScriptExternals.h"

// for MobiusContainer
#include "../MobiusInterface.h"

#include "MslTrack.h"
#include "LogicalTrack.h"
#include "TrackManager.h"
#include "TrackEvent.h"
// need to get TrackEvent pools out of here
#include "../midi/MidiPools.h"

#include "TrackMslHandler.h"

TrackMslHandler::TrackMslHandler(MobiusKernel* k, TrackManager* m) : variables(k)
{
    kernel = k;
    manager = m;
}

TrackMslHandler::~TrackMslHandler()
{
}

//////////////////////////////////////////////////////////////////////
//
// Query
//
//////////////////////////////////////////////////////////////////////

/**
 * Handle an MSL query on an internal variable.
 * Symbol queries will have been handled by MobiusKernel
 * and/or TrackManager.
 */
bool TrackMslHandler::mslQuery(MslQuery* query, MslTrack* track)
{
    return variables.get(query, track);
}

/**
 * This was the original implementation that used the old
 * ScriptInternalVariables.  Keep around for reference for awhile
 * in case this becomes necessary again.
 */
#if 0
bool TrackMslHandler::mslQuery(MslQuery* query)
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
// This is approximately the same as what ScriptWaitStatement::eval
// does for the old scripting engine.  It's rather complicated so after
// it is working consider ways to refactor this to make it more self contained.
//
//////////////////////////////////////////////////////////////////////

/**
 * todo: not handling the old "inPause" argument, need to find a syntax for
 * that and pass it down through the MslWait
 */
bool TrackMslHandler::mslWait(MslWait* wait, MslContextError* error)
{
    // todo: start depositing errors in MslError now that we have it
    (void)error;
    (void)wait;
    bool success = false;

    Trace(2, "TrackMslHandler::mslWait %s", MslWait::typeToKeyword(wait->type));
    Trace(2, "  amount %d number %d repeats %d",
          wait->amount, wait->number, wait->repeats);
    if (wait->forceNext)
      Trace(2, "  forceNext");

    MslTrack* track = manager->getMslTrack(wait->track);
    if (track == nullptr) {
        Trace(1, "TrackMslHandler: Invalid track number in MslWait %d", wait->track);
    }
    else {
        switch (wait->type) {
            
            case MslWaitSubcycle: {
                int subframes = track->getSubcycleFrames();
                if (subframes == 0)
                  Trace(1, "MSL: Wait duration Subcycle is not evailable in an empty loop");
                else if (wait->number == 0) {
                    // todo: find the subcycle we're in, then advance to the next one
                    // for repeats add the subcycle frame length
                }
                else {
                    int multiplier = wait->number - 1;
                    // repeats don't really make sense here, but ifyou have them
                    // it causes multiple iterations to reach the numbered subcycle
                    int frame = subframes * multiplier;
                    if (wait->repeats > 0) frame += (track->getLoopFrames() * wait->repeats);
                    success = track->scheduleWaitFrame(wait, frame);
                }
            }
                break;
            
            case MslWaitCycle: {
                int cycframes = track->getCycleFrames();
                if (cycframes == 0)
                  Trace(1, "MSL: Wait duration Subcycle is not evailable in an empty loop");
                else if (wait->number == 0) {
                    // todo: find the cycle we're in, then advance to the next one
                    // for repeats add the cycle frame length
                }
                else {
                    int multiplier = wait->number - 1;
                    int frame = cycframes * multiplier;
                    if (wait->repeats > 0) frame += (track->getLoopFrames() * wait->repeats);
                    success = track->scheduleWaitFrame(wait, frame);
                }
            }
                break;
                
            case MslWaitStart: {
                if (wait->repeats == 0)
                  track->scheduleWaitFrame(wait, 0);
                else {
                    // I suppose this could mean waiting for several loop passes
                }
            }
                break;
                
            case MslWaitEnd: {
                // todo: this is going to need something special, forget
                // how Mobius did this
                track->scheduleWaitFrame(wait, 0);
            }
                break;
                
            case MslWaitBeat: {
                // todo: Schedule an EventWait marked pending in our TrackEventList
                // then have TrackAdvancer look for it
                // for repeats, give it a countdown
            }
                break;
                
            case MslWaitBar: {
                // todo: Schedule an EventWait marked pending in our TrackEventList
                // then have TrackAdvancer look for it
            }
                break;

            case MslWaitFrame: {
                // straight and to the point
                int frames = wait->amount;
                // I suppose we can support repeats here, but you could also just mutltiply
                if (wait->repeats > 0) frames *= wait->repeats;
                int frame = track->getFrame() + frames;
                success = track->scheduleWaitFrame(wait, frame);
            }
                break;
                
            case MslWaitMsec: {
                int frames = getMsecFrames(track, wait->amount);
                if (wait->repeats > 0) frames *= wait->repeats;
                int frame = track->getFrame() + frames;
                success = track->scheduleWaitFrame(wait, frame);
            }
                break;
                
            case MslWaitSecond: {
                int frames = getSecondFrames(track, wait->amount);
                if (wait->repeats > 0) frames *= wait->repeats;
                int frame = track->getFrame() + frames;
                success = track->scheduleWaitFrame(wait, frame);
            }
                break;
                
            case MslWaitBlock: {
                // this we don't need to ask the track to schedule, just put
                // it on our event list and handle it
                LogicalTrack* lt = manager->getLogicalTrack(wait->track);
                success = lt->scheduleWait(wait);
            }
                break;
    
            case MslWaitLast: {
                // this is track engine specific
                success = track->scheduleWaitEvent(wait);
            }
                break;
            
            case MslWaitMarker:
            // from here down, they're iffy and may be not necessary
            // but the old scripts defined them
            case MslWaitSwitch:
            case MslWaitExternalStart:
            case MslWaitPulse:
            case MslWaitRealign:
            case MslWaitReturn:
            case MslWaitDriftCheck:
            default: {
                Trace(1, "TrackMslHandler: Wait type %s not implemented",
                      MslWait::typeToKeyword(wait->type));
            }
                break;
        }
    }
    return success;
}

/**
 * The target track is supposed to be passed in the MslWait
 * if the script is using an "in" statement for track scoping.
 * I guess this can deafult to the active track since everything
 * else works that way.
 *
 * new: The notion of "active track" only applies to Mobius audio
 * tracks.  For MIDI tracks it must be specified, and when Mobius
 * tracks become MslTracks, TrackManager must always resolve this.
 */
MslTrack* TrackMslHandler::getWaitTarget(MslWait* wait)
{
    MslTrack* track = nullptr;
    
    if (wait->track == 0) {
        Trace(1, "TrackMslHandler: Can't schedule wait without a track scope");
    }
    else {
        track = manager->getMslTrack(wait->track);
        if (track == nullptr) {
            Trace(1, "TrackMslHandler: MslWait with invalid track number %d", wait->track);
            // default to focused?
            //track = mobius->getTrack();
        }
    }
    return track;
}

/**
 * Return the number of frames represented by a millisecond.
 * Adjusted for the current playback rate.  
 * For accurate waits, you have to ensure that the rate can't
 * change while we're waiting.
 *
 * new: Revisit this I hate having to rely on rate adjusted track
 * advance for absolute time waits.  Instead, the event could be pending
 * with a countdown frame counter that decrements on each block at the
 * normal sampleRate and is independent of the track advance.
 */
int TrackMslHandler::getMsecFrames(MslTrack* track, int msecs)
{
    // old code uses the MSEC_TO_FRAMES macro which was defined
    // as this buried in MobiusConfig.h
    // #define MSEC_TO_FRAMES(msec) (int)(CD_SAMPLE_RATE * ((float)msec / 1000.0f))
    // that obviously doesn't work with variable sample rates so need to weed
    // out all uses of that old macro
	// should we ceil() here?
    int msecFrames = (int)((float)(manager->getContainer()->getSampleRate()) * ((float)msecs / 1000.0f));

    // adjust by playback rate
	//float rate = track->getEffectiveSpeed();
	float rate = track->getRate();
    int frames = (int)((float)msecFrames * rate);
    
    return frames;
}

int TrackMslHandler::getSecondFrames(MslTrack* track, int seconds)
{
    int secFrames = manager->getContainer()->getSampleRate();
    secFrames *= seconds;
	float rate = track->getRate();
    int frames = (int)((float)secFrames * rate);
    return frames;
}


// old implementation for the original wait model
#if 0

/**
 * The basic mechanism of scheduling an event at a specific position
 * This would be implemented by MidiTrack and MobiusTrackWrapper
 */
TrackEvent* TrackMslHandler::scheduleWaitAtFrame(MslWait* wait, MslTrack* track, int frame)
{
    // this isn't something that should be on the track
    // TrackManager should be maintaining them?
    // or maybe TrackScheduler should be in the way?
    // exposing TrackEventList means this can't be used for Mobius so
    // consider abstracting it under MslTrack?
    TrackEventList* events = track->getEventList();
    TrackEvent* e = manager->getPools()->newTrackEvent();
    e->type = TrackEvent::EventWait;
    e->wait = wait;
    e->frame = frame;
    
    events->add(e);

    // the old interpreter would set the event on the ScriptStack
    // here we save it in the Wait object itself
    // this actually isn't necessary MSL won't do anything with it,
    // it is only used to pass the previous async event IN when
    // setting up a "wait last"
    wait->coreEvent = e;
            
    // remember this while we're here, could be useful
    // we can trace it once we get back
    wait->coreEventFrame = frame;

    
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

    return e;
}
            
/**
 * A location wait waits for a subdivision of the loop identifified by number.
 */
bool TrackMslHandler::scheduleLocationWait(MslWait* wait)
{
    bool success = false;
    
    MslTrack* track = getWaitTarget(wait);
    if (track != nullptr) {
        int frame = calculateLocationFrame(wait, track);

        // if the location frame is negative it means that this
        // is an invalid location because the loop hasn't finished recording

        if (frame >= 0) {
            (void)scheduleWaitAtFrame(wait, track, frame);
            success = true;
        }
        else {
            // if we ignored it, is this "success"?
            // MslSession will error off if we return false
        }
    }
    return success;
}

/**
 * Old scripts call this wait type WAIT_ABSOLUTE
 *
 * hmm, mismatch between old and new scripts
 *
 * Old has: msec, frame, subcycle, cycle, loop
 *
 * New has: Start, End, Subcycle, Cycle, Beat, Bar, Marker
 *
 * There is no Loop because I think it's always the same
 * as "wait event loop" ?
 *
 * "wait location start" would be the same as "wait frame 0"
 * and "wait location end" would be the same as "wait frame loopFrames"
 *
 * guess it depends on exactly which side of the loop point it is on.
 * Need to verify this.  If we say "end" means "final frame of the loop
 * that is the same as the old "loop".
 *
 * The math was probably wrong here "end" would actually be loopFrames - 1
 * since there is no loopFrames frame, it wraps back to zero.
 *
 * old "Wait subcycle 3" meant "wait frame subcycleFrames * 3" which is the same
 * except if there are fewer than 3 subcycles in the loop.  Yes, that's right
 * it should wrap.  Could consider that an error or a noop.
 *
 * The problem with putting msec and frame back is that it makes it ambiguous
 * with duration msec and frame which are far more common.  
 */
int TrackMslHandler::calculateLocationFrame(MslWait* wait, MslTrack* track)
{
    int frame = -1;

    switch (wait->location) {

        // todo: WaitLocationMsec
        // old code did: frame = getMsecFrames(si, time);

        // todo: WaitLocationFrame
        // old code did: frame = time

        case WaitLocationStart: {
            // straight and to the point
            // this is new and mostly for completion
            frame = 0;
        }
            break;

        case WaitLocationEnd: {
            // this is new, and was intended to mean the moment
            // in time before the loop transition, logically it
            // is frame zero but if you immediately enter into an extension mode
            // it appends to the end rather than inserts at the beginning
            // needs thought and maybe some new scheduling concepts
            frame = 0;
        }
            break;

        case WaitLocationSubcycle: {
            // old comments:
            // Hmm, should the subcycle be relative to the
            // start of the loop or relative to the current cycle?
            // Start of the loop feels more natural.
            // If there aren't this many subcycles in a cycle, do
            // we spill over into the next cycle or round? Spill.

            // todo: if the loop is empty, this didn't work before but
            // we could be smarter now
            frame = track->getSubcycleFrames() * wait->value;
        }
            break;

        case WaitLocationCycle: {
            // same issues as Subcycle if the loop is empty
            frame = track->getCycleFrames() * wait->value;
        }
            break;

            // todo: WaitLocationLoop
            // old comments:
            // wait for the start of a particular loop
            // this is meaningless since there is only one loop, though
            // I supposed we could take this to mean whenever the
            // loop is triggered, that would be inconsistent with the
            // other absolute time values though.
            // Let this mean to wait for n iterations of the loop
            // frame = loop->getLoopFrames() * time;

        case WaitLocationBeat: {
            // new: if we're slave syncing and we know the beat length
            Trace(2, "Mobius: Wait location Beat not implemented");
        }
            break;
        case WaitLocationBar: {
            Trace(2, "Mobius: Wait location Bar not implemented");
        }
            break;
        case WaitLocationMarker: {
            Trace(2, "Mobius: Wait location Marker not implemented");
        }
            break;
            
        default:
            Trace(1, "MSL: Invalid wait location");
            break;
    }

    return frame;
}


bool TrackMslHandler::scheduleEventWait(MslWait* wait)
{
    bool success = false;
    
    MslTrack* track = getWaitTarget(wait);
    if (track != nullptr) {

        switch (wait->event) {

            case WaitEventLoop: {
                // new: should this be an event unit, or should it be
                // in Duration or Location instead?
                (void)scheduleWaitAtFrame(wait, track, 0);
                success = true;
            }
                break;

            case WaitEventEnd: {
                // new: similar issues as WaitEventLoop and LocationLoop
                (void)scheduleWaitAtFrame(wait, track, 0);
                success = true;
            }
                break;
                
            case WaitEventSubcycle: {
                // this was not in old, probably can consistnly be a Location wait
                // rather than event
            }
                break;

            case WaitEventCycle: {
                // this was not in old, probably can consistnly be a Location wait
                // rather than event
            }
                break;

            case WaitEventBeat: {
                // finally something that WAS in old code

                // old comments:
                // Various pending events that wait for Loop or
                // Synchronizer to active them at the right time.
                // !! TODO: Would be nice to wait for a specific pulse
                //Event* e = setupWaitEvent(si, 0);
                //e->pending = true;
                //e->fields.script.waitType = mWaitType;
                
            }
                break;
            case WaitEventBar: {
                // also in old code
                // schedule a pending wait and let Synchronizer activate it
                
            }
                break;

                // todo: WaitEventPulse
                // todo: old code had Start/End, here or is that a Location?
                
            case WaitEventExternalStart:
            case WaitEventRealign:
            case WaitEventReturn:
            case WaitEventDriftCheck: {
                // schedule a pending wait and wayt for Synchronizer to activate it
            }
                break;

                // new stuff
            case WaitEventMarker: {
            }
                break;


                // the next three are impotant and the meaning is clear
            case WaitEventLast: {
                // this works quite differently than MOS scripts
                // if a previous action scheduled an event, that would have
                // been returned in the UIAction and MSL would have remembered it
                // when it reaches a "wait last" it passes that event back down

                // this is a TrackEvent but it shouldn't matter
                void* requestId = wait->coreEvent;

                if (requestId == nullptr) {
                    // should not have gotten this far
                    wait->finished = true;
                }
                else {
                    // don't assume this object is still valid
                    // it almost always will be, but if there was any delay between
                    // the last action and the wait it could be gone

                    // breaking abstraction now so we can look for it as an event
                    TrackEventList* events = track->getEventList();
                    TrackEvent* event = static_cast<TrackEvent*>(requestId);

                    if (!events->isScheduled(event)) {
                        // yep, it's gone, don't treat this as an error
                        wait->finished = true;
                    }
                    else {
                        // and now we wait
                        event->wait = wait;
                        // set this while we're here though nothing uses it
                        wait->coreEventFrame = event->frame;
                    }
                }
                success = true;
            }
                break;
                
            case WaitEventSwitch: {
                TrackEventList* events = track->getEventList();
                TrackEvent* event = events->find(TrackEvent::EventSwitch);
                if (event != nullptr) {
                    wait->finished = true;
                }
                else {
                    event->wait = wait;
                }
                success = true;
            }
                break;
                
            case WaitEventBlock: {
                TrackEventList* events = track->getEventList();
                TrackEvent* e = manager->getPools()->newTrackEvent();
                e->type = TrackEvent::EventWait;
                e->wait = wait;
                e->pending = true;
                events->add(e);
                success = true;
            }
                break;
                
            default:
                Trace(1, "Mobius: Invalid Event wait");
                break;
        }

    }
    return success;
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
