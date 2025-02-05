/**
 * See core/MobiusMslVariableHandler for all of the old variables we might
 * want to support someday.
 */

#include <JuceHeader.h>

#include "../../script/MslContext.h"
#include "../../script/MslExternal.h"
#include "../../script/ScriptExternals.h"

#include "../../model/TrackState.h"

// for MobiusContainer
#include "../MobiusInterface.h"
#include "../MobiusKernel.h"
#include "../sync/SyncMaster.h"

#include "MslTrack.h"

#include "TrackMslVariableHandler.h"

TrackMslVariableHandler::TrackMslVariableHandler(MobiusKernel* k)
{
    kernel = k;
}

TrackMslVariableHandler::~TrackMslVariableHandler()
{
}

/**
 * A query comming from within an MSL script
 */
bool TrackMslVariableHandler::get(MslQuery* q, MslTrack* t)
{
    bool success = false;
    
    ScriptExternalType type = (ScriptExternalType)(q->external->type);
    
    if (type == ExtTypeVariable) {
        
        int intId = q->external->id;
        if (intId >= 0 && intId < (int)ExtMax) {
            ScriptExternalId id = (ScriptExternalId)intId;

            if (id == VarScopeTrack) {
                // this one is weird
                // If they didn't pass a scope in the query, I guess
                // this should fall back to the focused track?
                // * todo: this should be "scopeId" or "defaultScope" or "scopeNumber"
                if (q->scope > 0)
                  q->value.setInt(q->scope);
                else
                  q->value.setInt(kernel->getContainer()->getFocusedTrackIndex() + 1);
                success = true;
            }
            else {
                success = get(t, id, q->value);
            }
        }
    }
    return success;
}

/**
 * A query comming from system code
 */
bool TrackMslVariableHandler::get(VarQuery* q, MslTrack* t)
{
    return get(t, q->id, q->result);
}

/**
 * Common query dispatcher
 */
bool TrackMslVariableHandler::get(MslTrack* t, ScriptExternalId id, MslValue& result)
{
    bool success = true;
    
    switch (id) {
        case VarBlockFrames: getBlockFrames(t,result); break;
        case VarSampleRate: getSampleRate(t,result); break;
        case VarSampleFrames: getSampleFrames(t,result); break;
                    
        case VarLoopCount: getLoopCount(t,result); break;
        case VarLoopNumber: getLoopNumber(t,result); break;
        case VarLoopFrames: getLoopFrames(t,result); break;
        case VarLoopFrame: getLoopFrame(t,result); break;
        case VarCycleCount: getCycleCount(t,result); break;
        case VarCycleNumber: getCycleNumber(t,result); break;
        case VarCycleFrames: getCycleFrames(t,result); break;
        case VarCycleFrame: getCycleFrame(t,result); break;
        case VarSubcycleCount: getSubcycleCount(t,result); break;
        case VarSubcycleNumber: getSubcycleNumber(t,result); break;
        case VarSubcycleFrames: getSubcycleFrames(t,result); break;
        case VarSubcycleFrame: getSubcycleFrame(t,result); break;
        case VarModeName: getModeName(t,result); break;
        case VarIsRecording: getIsRecording(t,result);  break;
        case VarInOverdub: getInOverdub(t,result); break;
        case VarInHalfspeed: getInHalfspeed(t,result); break;
        case VarInReverse: getInReverse(t,result); break;
        case VarInMute: getInMute(t,result); break;
        case VarInPause: getInPause(t,result); break;
        case VarInRealign: getInRealign(t,result); break;
        case VarInReturn: getInReturn(t,result); break;

            // old name was just "rate"
        case VarPlaybackRate: getPlaybackRate(t,result); break;
                    
        case VarTrackCount: getTrackCount(t,result); break;
        case VarAudioTrackCount: getAudioTrackCount(t,result); break;
        case VarMidiTrackCount: getMidiTrackCount(t,result); break;
            // old name was "trackNumber"
        case VarActiveAudioTrack: getActiveTrack(t,result); break;
        case VarFocusedTrack: getFocusedTrackNumber(t,result); break;
                    
        case VarGlobalMute: getGlobalMute(t,result); break;

        case VarTrackSyncMaster: getTrackSyncMaster(t,result); break;
        case VarTransportMaster: getTransportMaster(t,result); break;
        case VarSyncTempo: getSyncTempo(t,result); break;
        case VarSyncRawBeat: getSyncRawBeat(t,result); break;
        case VarSyncBeat: getSyncBeat(t,result); break;
        case VarSyncBar: getSyncBar(t,result); break;

        default:
            success = false;
            break;
    }
    return success;
}

//////////////////////////////////////////////////////////////////////
//
// Loop State
//
//////////////////////////////////////////////////////////////////////

void TrackMslVariableHandler::getLoopCount(MslTrack* t, MslValue& v)
{
    v.setInt(t->getLoopCount());
}

void TrackMslVariableHandler::getLoopNumber(MslTrack* t, MslValue& v)
{
    v.setInt(t->getLoopIndex() + 1);
}

void TrackMslVariableHandler::getLoopFrames(MslTrack* t, MslValue& v)
{
    v.setInt(t->getFrames());
}

void TrackMslVariableHandler::getLoopFrame(MslTrack* t, MslValue& v)
{
    v.setInt(t->getFrame());
}

void TrackMslVariableHandler::getCycleCount(MslTrack* t, MslValue& v)
{
    v.setInt(t->getCycles());
}

void TrackMslVariableHandler::getCycleNumber(MslTrack* t, MslValue& v)
{
    int frame = t->getFrame();
    int cycleFrames = t->getCycleFrames();
    int cycleNumber = (int)(frame / cycleFrames);
    v.setInt(cycleNumber);
}

void TrackMslVariableHandler::getCycleFrames(MslTrack* t, MslValue& v)
{
    v.setInt(t->getCycleFrames());
}

void TrackMslVariableHandler::getCycleFrame(MslTrack* t, MslValue& v)
{
    int frame = t->getFrame();
    int cycleFrames = t->getCycleFrames();
    int cycleFrame = (int)(frame % cycleFrames);
    v.setInt(cycleFrame);
}

void TrackMslVariableHandler::getSubcycleCount(MslTrack* t, MslValue& v)
{
    v.setInt(t->getSubcycles());
}

/**
 * Old comments from Variable.cpp
 * The current subcycle number, relative to the current cycle.
 * !! Should this be relative to the start of the loop?
 */
void TrackMslVariableHandler::getSubcycleNumber(MslTrack* t, MslValue& v)
{
    int subcycles = t->getSubcycles();
    int frame = t->getFrame();
    int subcycleFrames = getSubcycleFrames(t);

    // absolute subcycle with in loop
    int subcycle = (int)(frame / subcycleFrames);

    // adjust to be relative to start of cycle
    subcycle %= subcycles;

    v.setInt(subcycle);
}

/**
 * This is a calculation Loop has but MslTrack doesn't
 */
int TrackMslVariableHandler::getSubcycleFrames(MslTrack* t)
{
    int subcycleFrames = 0;
	int cycleFrames = t->getCycleFrames();
	if (cycleFrames > 0) {
        int subcycles = t->getSubcycles();
		if (subcycles > 0)
		  subcycleFrames = cycleFrames / subcycles;
	}
    return subcycleFrames;
}

void TrackMslVariableHandler::getSubcycleFrames(MslTrack* t, MslValue& v)
{
    v.setInt(getSubcycleFrames(t));
}

void TrackMslVariableHandler::getSubcycleFrame(MslTrack* t, MslValue& v)
{
    int frame = t->getFrame();
    int subcycleFrames = getSubcycleFrames(t);
    int subcycleFrame = (int)(frame % subcycleFrames);
    v.setInt(subcycleFrame);
}

//////////////////////////////////////////////////////////////////////
//
// Track State
//
//////////////////////////////////////////////////////////////////////

void TrackMslVariableHandler::getModeName(MslTrack* t, MslValue& v)
{
    TrackState::Mode mode = t->getMode();

    // hack: minor modes are not conveyed by getMode, 
    // preferred way is to use inOverdub, inHalfspeed, inMute, etc.
    // unfortunately lots of old scripts do "if mode == "Overdub"
    // and this is also convenient for the case statement
    // this is also what the UI does but not sure I like it here
    // scripts need to be precise
    if (mode == TrackState::ModePlay) {
        if (t->isOverdub())
          mode = TrackState::ModeOverdub;
        else if (t->isMuted())
          mode = TrackState::ModeMute;
    }
    
    v.setString(TrackState::getModeName(mode));
}

/**
 * Loop has a flag for this, and MidiRecorder has basically the
 * same thing, but it isn't exposed
 */
void TrackMslVariableHandler::getIsRecording(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: isRecording not implemented");
    v.setBool(false);
}

void TrackMslVariableHandler::getInOverdub(MslTrack* t, MslValue& v)
{
    v.setBool(t->isOverdub());
}

/**
 * This is old, and it would be more useful to just know
 * the value of SpeedToggle
 */
void TrackMslVariableHandler::getInHalfspeed(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inHalfspeed not implemented");
    v.setBool(false);
}

void TrackMslVariableHandler::getInReverse(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inReverse not implemented");
    v.setBool(false);
}

void TrackMslVariableHandler::getInMute(MslTrack* t, MslValue& v)
{
    v.setBool(t->isMuted());
}

void TrackMslVariableHandler::getInPause(MslTrack* t, MslValue& v)
{
    v.setBool(t->isPaused());
}

/**
 * Is this really that interesting?  I guess for testing
 */
void TrackMslVariableHandler::getInRealign(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inRealign not implemented");
    v.setBool(false);
}

void TrackMslVariableHandler::getInReturn(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inReturn not implemented");
    v.setBool(false);
}

/**
 * !! This should be "speedStep"
 * "rate" was used a long time ago but that should be a float
 */
void TrackMslVariableHandler::getPlaybackRate(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: playbackRate not implemented");
    v.setInt(0);
}

/**
 * This is expected to be the total track count
 */
void TrackMslVariableHandler::getTrackCount(MslTrack* t, MslValue& v)
{
    (void)t;
    Session* s = kernel->getSession();
    v.setInt(s->getTrackCount());
}

void TrackMslVariableHandler::getAudioTrackCount(MslTrack* t, MslValue& v)
{
    (void)t;
    Session* s = kernel->getSession();
    v.setInt(s->getAudioTracks());
}

void TrackMslVariableHandler::getMidiTrackCount(MslTrack* t, MslValue& v)
{
    (void)t;
    Session* s = kernel->getSession();
    v.setInt(s->getMidiTracks());
}

/**
 * This we don't have with MIDI tracks
 * I don't think it's worthwhile to return this, though we could
 * rename this activeAudioTrack and have both sides handle it
 */
void TrackMslVariableHandler::getActiveTrack(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: activeTrack not implemented");
    v.setInt(0);
}

void TrackMslVariableHandler::getFocusedTrackNumber(MslTrack* t, MslValue& v)
{
    (void)t;
    v.setInt(kernel->getContainer()->getFocusedTrackIndex() + 1);
}

/**
 * Audio tracks have the flag on the Track which makes no sense
 * it should be derived from the mute state in all tracks.
 */
void TrackMslVariableHandler::getGlobalMute(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: globalMute not implemented");
    v.setBool(false);
}

//////////////////////////////////////////////////////////////////////
//
// Sync State
//
// Most (all?) of these go through pulsator so they could be done
// at either level.
//
//////////////////////////////////////////////////////////////////////

void TrackMslVariableHandler::getTrackSyncMaster(MslTrack* t, MslValue& v)
{
    (void)t;
    int tnum = kernel->getSyncMaster()->getTrackSyncMaster();
    v.setInt(tnum);
}

void TrackMslVariableHandler::getTransportMaster(MslTrack* t, MslValue& v)
{
    (void)t;
    // this could have been handled at either level
    int tnum = kernel->getSyncMaster()->getTransportMaster();
    v.setInt(tnum);
}

/**
 * Audio tracks save the sync source on each track and have
 * Synchronizer deal with it.  We could do something similar with TrackScheduler
 */
void TrackMslVariableHandler::getSyncTempo(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: syncTempo not implemented");
    v.setInt(0);
}

void TrackMslVariableHandler::getSyncRawBeat(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: syncRawBeat not implemented");
    v.setInt(0);
}

void TrackMslVariableHandler::getSyncBeat(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: syncBeat not implemented");
    v.setInt(0);
}

void TrackMslVariableHandler::getSyncBar(MslTrack* t, MslValue& v)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: syncBar not implemented");
    v.setInt(0);
}

//////////////////////////////////////////////////////////////////////
//
// Random
//
//////////////////////////////////////////////////////////////////////

void TrackMslVariableHandler::getBlockFrames(MslTrack* t, MslValue& v)
{
    (void)t;
    v.setInt(kernel->getContainer()->getBlockSize());
}

void TrackMslVariableHandler::getSampleRate(MslTrack* t, MslValue& v)
{
    (void)t;
    v.setInt(kernel->getContainer()->getSampleRate());
}

/**
 * The number of frames in the last sample we played.
 * Used in test scripts to set up waits for the sample to
 * finish playing.
 * Should be "lastSampleFrames" or something
 */
void TrackMslVariableHandler::getSampleFrames(MslTrack* t, MslValue& v)
{
    (void)t;
    int frames = (int)(kernel->getLastSampleFrames());
    v.setInt(frames);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
