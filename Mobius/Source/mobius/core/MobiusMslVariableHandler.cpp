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

bool MobiusMslVariableHandler::get(MslQuery* q)
{
    bool success = false;
    
    ScriptExternalType type = (ScriptExternalType)(q->external->type);
    
    if (type == ExtTypeVariable) {
        
        int intId = q->external->id;
        if (intId >= 0 && intId < (int)ExtMax) {
            success = true;
            ScriptExternalId id = (ScriptExternalId)intId;
            switch (id) {

                case VarBlockFrames: getBlockFrames(q); break;
                case VarSampleRate: getSampleRate(q); break;
                case VarSampleFrames: getSampleFrames(q); break;
                    
                case VarLoopCount: getLoopCount(q); break;
                case VarLoopNumber: getLoopNumber(q); break;
                case VarLoopFrames: getLoopFrames(q); break;
                case VarLoopFrame: getLoopFrame(q); break;
                case VarCycleCount: getCycleCount(q); break;
                case VarCycleNumber: getCycleNumber(q); break;
                case VarCycleFrames: getCycleFrames(q); break;
                case VarCycleFrame: getCycleFrame(q); break;
                case VarSubcycleCount: getSubcycleCount(q); break;
                case VarSubcycleNumber: getSubcycleNumber(q); break;
                case VarSubcycleFrames: getSubcycleFrames(q); break;
                case VarSubcycleFrame: getSubcycleFrame(q); break;

                    // old name was just "mode" may want to prefix that
                case VarModeName: getModeName(q); break;
                case VarIsRecording: getIsRecording(q);  break;
                case VarInOverdub: getInOverdub(q); break;
                case VarInHalfspeed: getInHalfspeed(q); break;
                case VarInReverse: getInReverse(q); break;
                case VarInMute: getInMute(q); break;
                case VarInPause: getInPause(q); break;
                case VarInRealign: getInRealign(q); break;
                case VarInReturn: getInReturn(q); break;

                    // old name was just "rate"
                case VarPlaybackRate: getPlaybackRate(q); break;
                    
                case VarTrackCount: getTrackCount(q); break;
                    // old name was "trackNumber"
                case VarActiveAudioTrack: getActiveTrack(q); break;
                case VarFocusedTrack: getFocusedTrack(q); break;
                case VarScopeTrack: getScopeTrack(q); break;
                    
                case VarGlobalMute: getGlobalMute(q); break;

                case VarTrackSyncMaster: getTrackSyncMaster(q); break;
                case VarOutSyncMaster: getOutSyncMaster(q); break;
                case VarSyncTempo: getSyncTempo(q); break;
                case VarSyncRawBeat: getSyncRawBeat(q); break;
                case VarSyncBeat: getSyncBeat(q); break;
                case VarSyncBar: getSyncBar(q); break;

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

/**
 * Obtain the track relevant for the query scope.
 * ah, now we have the audio/midi problem.  We either land
 * here and forward back up, or Kernel needs to pass this through
 * TrackManager and let it decide where to send it.
 */
Track* MobiusMslVariableHandler::getTrack(MslQuery* q)
{
    Track* track = nullptr;
    
    if (q->scope > 0) {
        int trackIndex = q->scope - 1;
        if (trackIndex < mobius->getTrackCount())
          track = mobius->getTrack(trackIndex);
        else {
            Trace(1, "Mobius: MSL variable handler scoped to a MIDI track");
        }
    }
    else {
        // this is supposed to mean the "focused" track
        // which can also be a MIDI track which needs to be handled higher
        // punt for now and use the active track
        track = mobius->getTrack();
    }
    return track;
}

void MobiusMslVariableHandler::getLoopCount(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) result = t->getLoopCount();
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getLoopNumber(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->getLoop()->getNumber();
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getLoopFrames(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = (int)(t->getLoop()->getFrames());
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getLoopFrame(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = (int)(t->getLoop()->getFrame());
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getCycleCount(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = (int)(t->getLoop()->getCycles());
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getCycleNumber(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        Loop* l = t->getLoop();
        long frame = l->getFrame();
        long cycleFrames = l->getCycleFrames();
        result = (int)(frame / cycleFrames);
    }
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getCycleFrames(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = (int)(t->getLoop()->getCycleFrames());
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getCycleFrame(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        Loop* l = t->getLoop();
        long frame = l->getFrame();
        long cycleFrames = l->getCycleFrames();
        result = (int)(frame % cycleFrames);
    }
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSubcycleCount(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        // sigh, Variable still uses Preset for this and so shall we
        Preset* p = t->getPreset();
        result = p->getSubcycles();
    }
    q->value.setInt(result);
}

/**
 * Old comments from Variable.cpp
 * The current subcycle number, relative to the current cycle.
 * !! Should this be relative to the start of the loop?
 */
void MobiusMslVariableHandler::getSubcycleNumber(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        Loop* l = t->getLoop();
        Preset* p = l->getPreset();
        long frame = l->getFrame();
        long subCycleFrames = l->getSubCycleFrames();

        // absolute subCycle with in loop
        long subCycle = frame / subCycleFrames;

        // adjust to be relative to start of cycle
        subCycle %= p->getSubcycles();

        result = (int)subCycle;
    }
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSubcycleFrames(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = (int)(t->getLoop()->getSubCycleFrames());
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSubcycleFrame(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        Loop* l = t->getLoop();
        long frame = l->getFrame();
        long subCycleFrames = l->getSubCycleFrames();
        result = (int)(frame % subCycleFrames);
    }
    q->value.setInt(result);
}

//////////////////////////////////////////////////////////////////////
//
// Track State
//
//////////////////////////////////////////////////////////////////////

void MobiusMslVariableHandler::getModeName(MslQuery* q)
{
    const char* result = nullptr;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->getLoop()->getMode()->getName();
    q->value.setString(result);
}

void MobiusMslVariableHandler::getIsRecording(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->getLoop()->isRecording();
    q->value.setBool(result);
}

void MobiusMslVariableHandler::getInOverdub(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->getLoop()->isOverdub();
    q->value.setBool(result);
}

/**
 * This is old, and it would be more useful to just know
 * the value of SpeedToggle
 */
void MobiusMslVariableHandler::getInHalfspeed(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = (t->getSpeedToggle() == -12);
    q->value.setBool(result);
}

void MobiusMslVariableHandler::getInReverse(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->getLoop()->isReverse();
    q->value.setBool(result);
}

void MobiusMslVariableHandler::getInMute(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->getLoop()->isMuteMode();
    q->value.setBool(result);
}

void MobiusMslVariableHandler::getInPause(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->getLoop()->isPaused();
    q->value.setBool(result);
}

/**
 * Is this really that interesting?  I guess for testing
 */
void MobiusMslVariableHandler::getInRealign(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr) {
        EventManager* em = t->getEventManager();
        Event* e = em->findEvent(RealignEvent);
        result = (e != NULL);
    }
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getInReturn(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr) {
        EventManager* em = t->getEventManager();
        Event* e = em->findEvent(ReturnEvent);
        result = (e != NULL);
    }
    q->value.setInt(result);
}

/**
 * !! This should be "speedStep"
 * "rate" was used a long time ago but that should be a float
 */
void MobiusMslVariableHandler::getPlaybackRate(MslQuery* q)
{
    int result = 1;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->getSpeedStep();
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getTrackCount(MslQuery* q)
{
    q->value.setInt(mobius->getTrackCount());
}

void MobiusMslVariableHandler::getActiveTrack(MslQuery* q)
{
    q->value.setInt(mobius->getTrack()->getDisplayNumber());
}

void MobiusMslVariableHandler::getFocusedTrack(MslQuery* q)
{
    q->value.setInt(mobius->getContainer()->getFocusedTrack() + 1);
}

/**
 * If they didn't pass a scope in the query, I guess
 * this should fall back to the focused track?
 */
void MobiusMslVariableHandler::getScopeTrack(MslQuery* q)
{
    if (q->scope > 0)
      q->value.setInt(q->scope);
    else
      q->value.setInt(mobius->getContainer()->getFocusedTrack() + 1);
}

/**
 * Why the fuck is this on the Track?
 * Is it replicated in all of them?
 */
void MobiusMslVariableHandler::getGlobalMute(MslQuery* q)
{
    bool result = false;
    Track* t = getTrack(q);
    if (t != nullptr)
      result = t->isGlobalMute();
    q->value.setInt(result);
}

//////////////////////////////////////////////////////////////////////
//
// Sync State
//
// Most (all?) of these go through pulsator so they could be done
// at either level.
//
//////////////////////////////////////////////////////////////////////

void MobiusMslVariableHandler::getTrackSyncMaster(MslQuery* q)
{
    int tnum = mobius->getContainer()->getPulsator()->getTrackSyncMaster();
    q->value.setInt(tnum);
}

void MobiusMslVariableHandler::getOutSyncMaster(MslQuery* q)
{
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
void MobiusMslVariableHandler::getSyncTempo(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        Synchronizer* s = t->getSynchronizer();
        float tempo = s->getTempo(t);
        result = (int)tempo;
    }
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSyncRawBeat(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        Synchronizer* s = t->getSynchronizer();
        result = s->getRawBeat(t);
    }
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSyncBeat(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        Synchronizer* s = t->getSynchronizer();
        result = s->getBeat(t);
    }
    q->value.setInt(result);
}

void MobiusMslVariableHandler::getSyncBar(MslQuery* q)
{
    int result = 0;
    Track* t = getTrack(q);
    if (t != nullptr) {
        Synchronizer* s = t->getSynchronizer();
        result = s->getBar(t);
    }
    q->value.setInt(result);
}

//////////////////////////////////////////////////////////////////////
//
// Random
//
//////////////////////////////////////////////////////////////////////

void MobiusMslVariableHandler::getBlockFrames(MslQuery* q)
{
    q->value.setInt(mobius->getContainer()->getBlockSize());
}

void MobiusMslVariableHandler::getSampleRate(MslQuery* q)
{
    q->value.setInt(mobius->getContainer()->getSampleRate());
}

/**
 * The number of frames in the last sample we played.
 * Used in test scripts to set up waits for the sample to
 * finish playing.
 * Should be "lastSampleFrames" or something
 */
void MobiusMslVariableHandler::getSampleFrames(MslQuery* q)
{
    int frames = (int)(mobius->getKernel()->getLastSampleFrames());
    q->value.setInt(frames);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
