/**
 * Internal ids for the built-in external functions and variables.
 * These are what is placed in the MslExternal when resolving
 * a reference from a script.
 *
 * Broken out of ScriptExternals.h to make it easier to access
 * variables without involving MslQuery and MslExternal.
 */

#pragma once

typedef enum {

    ExtNone,
    
    FuncGetMidiDeviceId,
    FuncMidiOut,
    FuncInstallUIElement,

    VarBlockFrames,
    VarSampleRate,
    VarSampleFrames,
    VarLoopCount,
    VarLoopNumber,
    VarLoopFrames,
    VarLoopFrame,
    VarCycleCount,
    VarCycleNumber,
    VarCycleFrames,
    VarCycleFrame,
    VarSubcycleCount,
    VarSubcycleNumber,
    VarSubcycleFrames,
    VarSubcycleFrame,
    VarModeName,
    VarIsRecording,
    VarInOverdub,
    VarInHalfspeed,
    VarInReverse,
    VarInMute,
    VarInPause,
    VarInRealign,
    VarInReturn,
    VarPlaybackRate,
    VarTrackCount,
    VarAudioTrackCount,
    VarMidiTrackCount,
    VarActiveAudioTrack,
    VarFocusedTrack,
    VarScopeTrack,
    VarGlobalMute,
    VarTrackSyncMaster,
    VarOutSyncMaster,
    VarSyncTempo,
    VarSyncRawBeat,
    VarSyncBeat,
    VarSyncBar,
    
    ExtMax

} ScriptExternalId;
