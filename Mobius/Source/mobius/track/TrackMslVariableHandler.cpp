/**
 * See core/MobiusMslVariableHandler for all of the old variables we might
 * want to support someday.
 */

#include <JuceHeader.h>

#include "../../script/MslContext.h"
#include "../../script/MslExternal.h"
#include "../../script/ScriptExternals.h"

// for MobiusContainer
#include "../MobiusInterface.h"
#include "../MobiusKernel.h"
#include "../../sync/Pulsator.h"

#include "AbstractTrack.h"

#include "TrackMslVariableHandler.h"

TrackMslVariableHandler::TrackMslVariableHandler(MobiusKernel* k)
{
    kernel = k;
}

TrackMslVariableHandler::~TrackMslVariableHandler()
{
}

bool TrackMslVariableHandler::get(MslQuery* q, AbstractTrack* t)
{
    bool success = false;
    
    ScriptExternalType type = (ScriptExternalType)(q->external->type);
    
    if (type == ExtTypeVariable) {
        
        int intId = q->external->id;
        if (intId >= 0 && intId < (int)ExtMax) {
            success = true;
            ScriptExternalId id = (ScriptExternalId)intId;
            switch (id) {

                case VarBlockFrames: getBlockFrames(q,t); break;
                case VarSampleRate: getSampleRate(q,t); break;
                case VarSampleFrames: getSampleFrames(q,t); break;
                    
                case VarLoopCount: getLoopCount(q,t); break;
                case VarLoopNumber: getLoopNumber(q,t); break;
                case VarLoopFrames: getLoopFrames(q,t); break;
                case VarLoopFrame: getLoopFrame(q,t); break;
                case VarCycleCount: getCycleCount(q,t); break;
                case VarCycleNumber: getCycleNumber(q,t); break;
                case VarCycleFrames: getCycleFrames(q,t); break;
                case VarCycleFrame: getCycleFrame(q,t); break;
                case VarSubcycleCount: getSubcycleCount(q,t); break;
                case VarSubcycleNumber: getSubcycleNumber(q,t); break;
                case VarSubcycleFrames: getSubcycleFrames(q,t); break;
                case VarSubcycleFrame: getSubcycleFrame(q,t); break;

                    // old name was just "mode" may want to prefix that
                case VarModeName: getModeName(q,t); break;
                case VarIsRecording: getIsRecording(q,t);  break;
                case VarInOverdub: getInOverdub(q,t); break;
                case VarInHalfspeed: getInHalfspeed(q,t); break;
                case VarInReverse: getInReverse(q,t); break;
                case VarInMute: getInMute(q,t); break;
                case VarInPause: getInPause(q,t); break;
                case VarInRealign: getInRealign(q,t); break;
                case VarInReturn: getInReturn(q,t); break;

                    // old name was just "rate"
                case VarPlaybackRate: getPlaybackRate(q,t); break;
                    
                case VarTrackCount: getTrackCount(q,t); break;
                case VarAudioTrackCount: getAudioTrackCount(q,t); break;
                case VarMidiTrackCount: getMidiTrackCount(q,t); break;
                    // old name was "trackNumber"
                case VarActiveAudioTrack: getActiveTrack(q,t); break;
                case VarFocusedTrack: getFocusedTrack(q,t); break;
                case VarScopeTrack: getScopeTrack(q,t); break;
                    
                case VarGlobalMute: getGlobalMute(q,t); break;

                case VarTrackSyncMaster: getTrackSyncMaster(q,t); break;
                case VarOutSyncMaster: getOutSyncMaster(q,t); break;
                case VarSyncTempo: getSyncTempo(q,t); break;
                case VarSyncRawBeat: getSyncRawBeat(q,t); break;
                case VarSyncBeat: getSyncBeat(q,t); break;
                case VarSyncBar: getSyncBar(q,t); break;

                default:
                    success = false;
                    break;
            }
        }
    }
    return success;
}

//////////////////////////////////////////////////////////////////////
//
// Loop State
//
//////////////////////////////////////////////////////////////////////

void TrackMslVariableHandler::getLoopCount(MslQuery* q, AbstractTrack* t)
{
    q->value.setInt(t->getLoopCount());
}

void TrackMslVariableHandler::getLoopNumber(MslQuery* q, AbstractTrack* t)
{
    q->value.setInt(t->getLoopIndex() + 1);
}

void TrackMslVariableHandler::getLoopFrames(MslQuery* q, AbstractTrack* t)
{
    q->value.setInt(t->getLoopFrames());
}

void TrackMslVariableHandler::getLoopFrame(MslQuery* q, AbstractTrack* t)
{
    q->value.setInt(t->getFrame());
}

void TrackMslVariableHandler::getCycleCount(MslQuery* q, AbstractTrack* t)
{
    q->value.setInt(t->getCycles());
}

void TrackMslVariableHandler::getCycleNumber(MslQuery* q, AbstractTrack* t)
{
    int frame = t->getFrame();
    int cycleFrames = t->getCycleFrames();
    int cycleNumber = (int)(frame / cycleFrames);
    q->value.setInt(cycleNumber);
}

void TrackMslVariableHandler::getCycleFrames(MslQuery* q, AbstractTrack* t)
{
    q->value.setInt(t->getCycleFrames());
}

void TrackMslVariableHandler::getCycleFrame(MslQuery* q, AbstractTrack* t)
{
    int frame = t->getFrame();
    int cycleFrames = t->getCycleFrames();
    int cycleFrame = (int)(frame % cycleFrames);
    q->value.setInt(cycleFrame);
}

void TrackMslVariableHandler::getSubcycleCount(MslQuery* q, AbstractTrack* t)
{
    q->value.setInt(t->getSubcycles());
}

/**
 * Old comments from Variable.cpp
 * The current subcycle number, relative to the current cycle.
 * !! Should this be relative to the start of the loop?
 */
void TrackMslVariableHandler::getSubcycleNumber(MslQuery* q, AbstractTrack* t)
{
    int subcycles = t->getSubcycles();
    int frame = t->getFrame();
    int subcycleFrames = getSubcycleFrames(t);

    // absolute subcycle with in loop
    int subcycle = (int)(frame / subcycleFrames);

    // adjust to be relative to start of cycle
    subcycle %= subcycles;

    q->value.setInt(subcycle);
}

/**
 * This is a calculation Loop has but AbstractTrack doesn't
 */
int TrackMslVariableHandler::getSubcycleFrames(AbstractTrack* t)
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

void TrackMslVariableHandler::getSubcycleFrames(MslQuery* q, AbstractTrack* t)
{
    q->value.setInt(getSubcycleFrames(t));
}

void TrackMslVariableHandler::getSubcycleFrame(MslQuery* q, AbstractTrack* t)
{
    int frame = t->getFrame();
    int subcycleFrames = getSubcycleFrames(t);
    int subcycleFrame = (int)(frame % subcycleFrames);
    q->value.setInt(subcycleFrame);
}

//////////////////////////////////////////////////////////////////////
//
// Track State
//
//////////////////////////////////////////////////////////////////////

void TrackMslVariableHandler::getModeName(MslQuery* q, AbstractTrack* t)
{
    MobiusState::Mode mode = t->getMode();
    q->value.setString(MobiusState::getModeName(mode));
}

/**
 * Loop has a flag for this, and MidiRecorder has basically the
 * same thing, but it isn't exposed
 */
void TrackMslVariableHandler::getIsRecording(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: isRecording not implemented");
    q->value.setBool(false);
}

void TrackMslVariableHandler::getInOverdub(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    // in general, MidiTrack doesn't have a lot of state exposure beyond
    // the full MobiusState
    Trace(1, "TrackMslVariableHandler: inOverdub not implemented");
    q->value.setBool(false);
}

/**
 * This is old, and it would be more useful to just know
 * the value of SpeedToggle
 */
void TrackMslVariableHandler::getInHalfspeed(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inHalfspeed not implemented");
    q->value.setBool(false);
}

void TrackMslVariableHandler::getInReverse(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inReverse not implemented");
    q->value.setBool(false);
}

void TrackMslVariableHandler::getInMute(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inMute not implemented");
    q->value.setBool(false);
}

void TrackMslVariableHandler::getInPause(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    q->value.setBool(t->isPaused());
}

/**
 * Is this really that interesting?  I guess for testing
 */
void TrackMslVariableHandler::getInRealign(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inRealign not implemented");
    q->value.setBool(false);
}

void TrackMslVariableHandler::getInReturn(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: inReturn not implemented");
    q->value.setBool(false);
}

/**
 * !! This should be "speedStep"
 * "rate" was used a long time ago but that should be a float
 */
void TrackMslVariableHandler::getPlaybackRate(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: playbackRate not implemented");
    q->value.setInt(0);
}

/**
 * This is expected to be the total track count
 */
void TrackMslVariableHandler::getTrackCount(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    int total = kernel->getAudioTrackCount() + kernel->getMidiTrackCount();
    q->value.setInt(total);
}

void TrackMslVariableHandler::getAudioTrackCount(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    q->value.setInt(kernel->getAudioTrackCount());
}

void TrackMslVariableHandler::getMidiTrackCount(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    q->value.setInt(kernel->getMidiTrackCount());
}

/**
 * This we don't have with MIDI tracks
 * I don't think it's worthwhile to return this, though we could
 * rename this activeAudioTrack and have both sides handle it
 */
void TrackMslVariableHandler::getActiveTrack(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: activeTrack not implemented");
    q->value.setInt(0);
}

void TrackMslVariableHandler::getFocusedTrack(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    q->value.setInt(kernel->getContainer()->getFocusedTrack() + 1);
}

/**
 * If they didn't pass a scope in the query, I guess
 * this should fall back to the focused track?
 */
void TrackMslVariableHandler::getScopeTrack(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    if (q->scope > 0)
      q->value.setInt(q->scope);
    else
      q->value.setInt(kernel->getContainer()->getFocusedTrack() + 1);
}

/**
 * Audio tracks have the flag on the Track which makes no sense
 * it should be derived from the mute state in all tracks.
 */
void TrackMslVariableHandler::getGlobalMute(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: globalMute not implemented");
    q->value.setBool(false);
}

//////////////////////////////////////////////////////////////////////
//
// Sync State
//
// Most (all?) of these go through pulsator so they could be done
// at either level.
//
//////////////////////////////////////////////////////////////////////

void TrackMslVariableHandler::getTrackSyncMaster(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    int tnum = kernel->getContainer()->getPulsator()->getTrackSyncMaster();
    q->value.setInt(tnum);
}

void TrackMslVariableHandler::getOutSyncMaster(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    // this could have been handled at either level
    int tnum = kernel->getContainer()->getPulsator()->getOutSyncMaster();
    q->value.setInt(tnum);
}

/**
 * Audio tracks save the sync source on each track and have
 * Synchronizer deal with it.  We could do something similar with TrackScheduler
 */
void TrackMslVariableHandler::getSyncTempo(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: syncTempo not implemented");
    q->value.setInt(0);
}

void TrackMslVariableHandler::getSyncRawBeat(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: syncRawBeat not implemented");
    q->value.setInt(0);
}

void TrackMslVariableHandler::getSyncBeat(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: syncBeat not implemented");
    q->value.setInt(0);
}

void TrackMslVariableHandler::getSyncBar(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    Trace(1, "TrackMslVariableHandler: syncBar not implemented");
    q->value.setInt(0);
}

//////////////////////////////////////////////////////////////////////
//
// Random
//
//////////////////////////////////////////////////////////////////////

void TrackMslVariableHandler::getBlockFrames(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    q->value.setInt(kernel->getContainer()->getBlockSize());
}

void TrackMslVariableHandler::getSampleRate(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    q->value.setInt(kernel->getContainer()->getSampleRate());
}

/**
 * The number of frames in the last sample we played.
 * Used in test scripts to set up waits for the sample to
 * finish playing.
 * Should be "lastSampleFrames" or something
 */
void TrackMslVariableHandler::getSampleFrames(MslQuery* q, AbstractTrack* t)
{
    (void)t;
    int frames = (int)(kernel->getLastSampleFrames());
    q->value.setInt(frames);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
