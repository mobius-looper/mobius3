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

#include "../../script/MslContext.h"
#include "../../script/MslExternal.h"
#include "../../script/ScriptExternals.h"

#include "Mobius.h"
#include "Track.h"

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

void MobiusMslVariableHandler::getLoopCount(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getLoopNumber(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getLoopFrames(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getLoopFrame(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getCycleCount(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getCycleNumber(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getCycleFrames(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getCycleFrame(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSubcycleCount(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSubcycleNumber(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSubcycleFrames(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSubcycleFrame(MslQuery* q)
{
    q->value.setInt(0);
}


//////////////////////////////////////////////////////////////////////
//
// Track State
//
//////////////////////////////////////////////////////////////////////

void MobiusMslVariableHandler::getModeName(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getIsRecording(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getInOverdub(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getInHalfspeed(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getInReverse(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getInMute(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getInPause(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getInRealign(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getInReturn(MslQuery* q)
{
    q->value.setInt(0);
}


void MobiusMslVariableHandler::getPlaybackRate(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getTrackCount(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getActiveTrack(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getFocusedTrack(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getScopeTrack(MslQuery* q)
{
    q->value.setInt(0);
}

                    
void MobiusMslVariableHandler::getGlobalMute(MslQuery* q)
{
    q->value.setInt(0);
}

//////////////////////////////////////////////////////////////////////
//
// Sync State
//
//////////////////////////////////////////////////////////////////////

void MobiusMslVariableHandler::getTrackSyncMaster(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getOutSyncMaster(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSyncTempo(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSyncRawBeat(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSyncBeat(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSyncBar(MslQuery* q)
{
    q->value.setInt(0);
}


//////////////////////////////////////////////////////////////////////
//
// Random
//
//////////////////////////////////////////////////////////////////////


void MobiusMslVariableHandler::getBlockFrames(MslQuery* q)
{
    q->value.setInt(0);
}

void MobiusMslVariableHandler::getSampleFrames(MslQuery* q)
{
    q->value.setInt(0);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
