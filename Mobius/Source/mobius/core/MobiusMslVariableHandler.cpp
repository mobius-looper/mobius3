/**
 * Variables that require thought
 *
 * Most of these are only for use in the test scripts.  Which is unfortunate since I'd like
 * to use a lot of the old scripts.
 *
 * sustainCount
 * clickCount
 *   Use ScriptInterpreter
 *   These will require access to the MslSession and state maintained
 *   for the sustain duration and clicks.
 *
 * triggerSource
 * triggerNumber
 * triggerValue
 * triggerVelocity
 * triggerOffset
 * midiType
 * midiChannel
 * midiNumber
 * midiValue
 *   Use ScriptInterpreter
 *   MslSession doesn't save anything about the action that caused it, might be interesting.
 *
 * returnCode
 *  "The return code of the last KernelEvent.  Currently used by the Prompt statement
 *   to convey the selected button"
 *  This we don't need, if you want to do prompting can have new ways to do that that
 *  use thread transitions rather than KernelEvents.
 *
 * noExternalAudio
 *  Controls whether audio from the container is suppressed during testing.
 *  This one is read/write and probably better as a hidden parameter?
 *
 * cycleCount
 *   This is read/write and it would be useful to change it.
 *   Could again be a hidden parameter or a function which suits it better maybe.
 *
 * effectiveFeedback
 *   The value of the feedback currently being applied.  Either Feedback or AltFeedback
 *   or zero in Insert and Replace.  I think used in test scripts.
 *
 * nextEvent
 *  Type name of the next event.
 * nextEventFunction
 *  Function name associated with the next event
 *
 * nextLoop
 *   Number of the next loop if we're in loop switch mode.
 *   ! This is something that would be useful to modify without having to use a function
 *
 * eventSummary
 *   Returns a string representation of all scheduled events for testing
 *
 * rawSpeed
 * rawRate
 * rawPitch
 *   Different representations for playback rate, probably testing
 *
 * speedToggle
 * speedSequenceIndex
 * pitchSequenceIndex
 *   Various state related to pitch/speed
 *
 * historyFrames
 *   Total number of frames in all layers.  Used to determine the relative location of
 *   the loop window.
 *
 * windowOffset
 *    Offset in frames of the current loop widow within historyFrames
 *
 * solo
 *   True if the track will be unmuted when Global Mute mode is over.
 *   
 * syncRawBeat
 * syncBeat
 * syncBar
 * syncPulses
 * syncPulse
 * syncPulseFrames
 * syncLoopFrames
 * syncAudioFrame
 * syncDrift
 * syncAverageDrift
 * syncDriftChecks
 * syncCorrections
 * syncDealign
 * syncPreRealignFrame
 * syncCyclePulses
 *    Various things related to sync.  Some of this is changing due to Pulsator
 *    redesign when ready.
 *
 * syncOutTempo
 * syncOutRawBeat
 * syncOutBeat
 * syncOutBar
 * syncOutSending
 * syncOutStarted
 * syncOutStarts
 * syncInTempo
 * syncInRawBeat
 * syncInBeat
 * syncInBar
 * syncInReceiving
 * syncInStarted
 * syncHostTempo
 * syncHostRawBeat
 * syncHostBeat
 * syncHostBar
 * syncHostReceiving
 *   Some of these might be interesting
 *
 * installationDirectory
 * configurationDirectory
 *   Not necessary
 *
 */

#include <JuceHeader.h>

#include "../../model/Preset.h"

#include "../../script/MslContext.h"
#include "../../script/MslExternal.h"
#include "../../script/ScriptExternals.h"

// for MobiusContainer
#include "../MobiusInterface.h"
#include "../MobiusKernel.h"
#include "../../sync/Pulsator.h"

#include "Mobius.h"
#include "Track.h"
#include "Loop.h"
#include "Synchronizer.h"
#include "EventManager.h"
#include "Event.h"
#include "Mode.h"

#include "MobiusMslVariableHandler.h"

MobiusMslVariableHandler::MobiusMslVariableHandler(Mobius* m)
{
    mobius = m;
}

MobiusMslVariableHandler::~MobiusMslVariableHandler()
{
}

bool MobiusMslVariableHandler::get(MslQuery* q, Track* t)
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

void MobiusMslVariableHandler::getLoopCount(MslQuery* q, Track* t)
{
    q->value.setInt(t->getLoopCount());
}

void MobiusMslVariableHandler::getLoopNumber(MslQuery* q, Track* t)
{
    q->value.setInt(t->getLoop()->getNumber());
}

void MobiusMslVariableHandler::getLoopFrames(MslQuery* q, Track* t)
{
    q->value.setInt((int)(t->getLoop()->getFrames()));
}

void MobiusMslVariableHandler::getLoopFrame(MslQuery* q, Track* t)
{
    q->value.setInt((int)(t->getLoop()->getFrame()));
}

void MobiusMslVariableHandler::getCycleCount(MslQuery* q, Track* t)
{
    q->value.setInt((int)(t->getLoop()->getCycles()));
}

void MobiusMslVariableHandler::getCycleNumber(MslQuery* q, Track* t)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long cycleFrames = l->getCycleFrames();
    int result = (int)(frame / cycleFrames);
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getCycleFrames(MslQuery* q, Track* t)
{
    q->value.setInt((int)(t->getLoop()->getCycleFrames()));
}

void MobiusMslVariableHandler::getCycleFrame(MslQuery* q, Track* t)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long cycleFrames = l->getCycleFrames();
    int result = (int)(frame % cycleFrames);
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSubcycleCount(MslQuery* q, Track* t)
{
    // sigh, Variable still uses Preset for this and so shall we
    Preset* p = t->getPreset();
    int result = p->getSubcycles();
    q->value.setInt(result);
}

/**
 * Old comments from Variable.cpp
 * The current subcycle number, relative to the current cycle.
 * !! Should this be relative to the start of the loop?
 */
void MobiusMslVariableHandler::getSubcycleNumber(MslQuery* q, Track* t)
{
    Loop* l = t->getLoop();
    Preset* p = l->getPreset();
    long frame = l->getFrame();
    long subCycleFrames = l->getSubCycleFrames();

    // absolute subCycle with in loop
    long subCycle = frame / subCycleFrames;

    // adjust to be relative to start of cycle
    subCycle %= p->getSubcycles();

    int result = (int)subCycle;

    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSubcycleFrames(MslQuery* q, Track* t)
{
    q->value.setInt((int)(t->getLoop()->getSubCycleFrames()));
}

void MobiusMslVariableHandler::getSubcycleFrame(MslQuery* q, Track* t)
{
    Loop* l = t->getLoop();
    long frame = l->getFrame();
    long subCycleFrames = l->getSubCycleFrames();
    int result = (int)(frame % subCycleFrames);
    q->value.setInt(result);
}

//////////////////////////////////////////////////////////////////////
//
// Track State
//
//////////////////////////////////////////////////////////////////////

void MobiusMslVariableHandler::getModeName(MslQuery* q, Track* t)
{
    q->value.setString(t->getLoop()->getMode()->getName());
}

void MobiusMslVariableHandler::getIsRecording(MslQuery* q, Track* t)
{
    q->value.setBool(t->getLoop()->isRecording());
}


void MobiusMslVariableHandler::getInOverdub(MslQuery* q, Track* t)
{
    q->value.setBool(t->getLoop()->isOverdub());
}

/**
 * This is old, and it would be more useful to just know
 * the value of SpeedToggle
 */
void MobiusMslVariableHandler::getInHalfspeed(MslQuery* q, Track* t)
{
    bool result = (t->getSpeedToggle() == -12);
    q->value.setBool(result);
}

void MobiusMslVariableHandler::getInReverse(MslQuery* q, Track* t)
{
    bool result = t->getLoop()->isReverse();
    q->value.setBool(result);
}

void MobiusMslVariableHandler::getInMute(MslQuery* q, Track* t)
{
    bool result = t->getLoop()->isMuteMode();
    q->value.setBool(result);
}

void MobiusMslVariableHandler::getInPause(MslQuery* q, Track* t)
{
    bool result = t->getLoop()->isPaused();
    q->value.setBool(result);
}

/**
 * Is this really that interesting?  I guess for testing
 */
void MobiusMslVariableHandler::getInRealign(MslQuery* q, Track* t)
{
    EventManager* em = t->getEventManager();
    Event* e = em->findEvent(RealignEvent);
    bool result = (e != NULL);
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getInReturn(MslQuery* q, Track* t)
{
    EventManager* em = t->getEventManager();
    Event* e = em->findEvent(ReturnEvent);
    bool result = (e != NULL);
    q->value.setInt(result);
}

/**
 * !! This should be "speedStep"
 * "rate" was used a long time ago but that should be a float
 */
void MobiusMslVariableHandler::getPlaybackRate(MslQuery* q, Track* t)
{
    q->value.setInt(t->getSpeedStep());
}

/**
 * Here we have a problem.
 * Old scripts used this name to refer only to audio tracks
 * but now we're starting to use it for the combined track count.
 */
void MobiusMslVariableHandler::getTrackCount(MslQuery* q, Track* t)
{
    (void)t;
    int total = mobius->getTrackCount() + mobius->getKernel()->getMidiTrackCount();
    q->value.setInt(total);
}

void MobiusMslVariableHandler::getAudioTrackCount(MslQuery* q, Track* t)
{
    (void)t;
    q->value.setInt(mobius->getTrackCount());
}

void MobiusMslVariableHandler::getMidiTrackCount(MslQuery* q, Track* t)
{
    (void)t;
    q->value.setInt(mobius->getKernel()->getMidiTrackCount());
}

void MobiusMslVariableHandler::getActiveTrack(MslQuery* q, Track* t)
{
    (void)t;
    q->value.setInt(mobius->getTrack()->getDisplayNumber());
}

void MobiusMslVariableHandler::getFocusedTrack(MslQuery* q, Track* t)
{
    (void)t;
    q->value.setInt(mobius->getContainer()->getFocusedTrack() + 1);
}

/**
 * If they didn't pass a scope in the query, I guess
 * this should fall back to the focused track?
 */
void MobiusMslVariableHandler::getScopeTrack(MslQuery* q, Track* t)
{
    (void)t;
    if (q->scope > 0)
      q->value.setInt(q->scope);
    else
      q->value.setInt(mobius->getContainer()->getFocusedTrack() + 1);
}

/**
 * Why the fuck is this on the Track?
 * Is it replicated in all of them?
 */
void MobiusMslVariableHandler::getGlobalMute(MslQuery* q, Track* t)
{
    q->value.setInt(t->isGlobalMute());
}

//////////////////////////////////////////////////////////////////////
//
// Sync State
//
// Most (all?) of these go through pulsator so they could be done
// at either level.
//
//////////////////////////////////////////////////////////////////////

void MobiusMslVariableHandler::getTrackSyncMaster(MslQuery* q, Track* t)
{
    (void)t;
    int tnum = mobius->getContainer()->getPulsator()->getTrackSyncMaster();
    q->value.setInt(tnum);
}

void MobiusMslVariableHandler::getOutSyncMaster(MslQuery* q, Track* t)
{
    (void)t;
    // this could have been handled at either level
    int tnum = mobius->getContainer()->getPulsator()->getOutSyncMaster();
    q->value.setInt(tnum);
}

/**
 * This is a float but we have historically truncated it
 * Need more options here
 *
 * It's not really the tempo of the track, it's the tempo
 * of the sync source the track is following.
 */
void MobiusMslVariableHandler::getSyncTempo(MslQuery* q, Track* t)
{
    Synchronizer* s = t->getSynchronizer();
    float tempo = s->getTempo(t);
    int result = (int)tempo;
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSyncRawBeat(MslQuery* q, Track* t)
{
    Synchronizer* s = t->getSynchronizer();
    int result = s->getRawBeat(t);
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSyncBeat(MslQuery* q, Track* t)
{
    Synchronizer* s = t->getSynchronizer();
    int result = s->getBeat(t);
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSyncBar(MslQuery* q, Track* t)
{
    Synchronizer* s = t->getSynchronizer();
    int result = s->getBar(t);
    q->value.setInt(result);
}

//////////////////////////////////////////////////////////////////////
//
// Random
//
//////////////////////////////////////////////////////////////////////

void MobiusMslVariableHandler::getBlockFrames(MslQuery* q, Track* t)
{
    (void)t;
    q->value.setInt(mobius->getContainer()->getBlockSize());
}

void MobiusMslVariableHandler::getSampleRate(MslQuery* q, Track* t)
{
    (void)t;
    q->value.setInt(mobius->getContainer()->getSampleRate());
}

/**
 * The number of frames in the last sample we played.
 * Used in test scripts to set up waits for the sample to
 * finish playing.
 * Should be "lastSampleFrames" or something
 */
void MobiusMslVariableHandler::getSampleFrames(MslQuery* q, Track* t)
{
    (void)t;
    int frames = (int)(mobius->getKernel()->getLastSampleFrames());
    q->value.setInt(frames);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
