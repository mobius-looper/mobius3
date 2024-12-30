
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
 *
 * We have no "global" script variables at the moment but when you
 * do handle them here.
 */
bool TrackMslHandler::mslQuery(LogicalTrack* track, MslQuery* query)
{
    bool success = false;

    // not all tracks support MSL
    MslTrack* mt = track->getMslTrack();
    if (mt != nullptr)
      success = variables.get(query, mt);
    else {
        // do we want to trace warnings about this?
        // or just silently return null
    }
    return success;
}

/**
 * A different form of variable query that comes from system code rather than
 * from within a script.  Used in a few places for the UI/Shell to access
 * variables without having to punch holes in MobiusInterface every time.
 */
bool TrackMslHandler::varQuery(LogicalTrack* track, VarQuery* query)
{
    bool success = false;
    MslTrack* mt = track->getMslTrack();
    if (mt != nullptr)
      success = variables.get(query, mt);
    else {
        // do we want to trace warnings about this?
        // or just silently return null
    }
    return success;
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
bool TrackMslHandler::mslWait(LogicalTrack* ltrack, MslWait* wait, MslContextError* error)
{
    // todo: start depositing errors in MslError now that we have it
    (void)error;
    bool success = false;

    Trace(2, "TrackMslHandler::mslWait %s", MslWait::typeToKeyword(wait->type));
    Trace(2, "  amount %d number %d repeats %d",
          wait->amount, wait->number, wait->repeats);
    if (wait->forceNext)
      Trace(2, "  forceNext");

    MslTrack* track = ltrack->getMslTrack();
    if (track == nullptr) {
        // they asked for a wait on a non-MSL track, this is a bit more
        // serious than a query
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
                    if (wait->repeats > 0) frame += (track->getFrames() * wait->repeats);
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
                    if (wait->repeats > 0) frame += (track->getFrames() * wait->repeats);
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
