/**
 * Implementataions of external symbols provided only for the script environment.
 * Most externals correspond go Mobius core functions that are visible in bindings.
 * These are support functions just for script authors.
 */

#pragma once

#include <JuceHeader.h>

/**
 * Type constants used in the MslExternal object when resolving external references
 */
typedef enum {

    ExtTypeSymbol,
    ExtTypeFunction,
    ExtTypeVariable,
    ExtTypeOldVariable
    
} ScriptExternalType;

/**
 * Internal ids for the built-in external functions and variables.
 * These are what is placed in the MslExternal when resolving
 * a reference from a script.
 */
typedef enum {

    ExtNone,
    
    FuncGetMidiDeviceId,
    FuncMidiOut,

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

/**
 * This identifies which "side" the function should run on.
 * Most should be independent but some may have assumptions about
 * their context.
 */
typedef enum {

    ScriptContextNone,
    ScriptContextShell,
    ScriptContextKernel
    
} ScriptContext;
    
/**
 * Structure that associates a ScriptExternalId with it's name.
 */
class ScriptExternalDefinition
{
  public:
    const char* name;
    ScriptExternalId id;
    ScriptContext context;
    bool function;

    static void dump();
};

/**
 * Static array of external function definitions.
 */
extern ScriptExternalDefinition ScriptExternalDefinitions[];

/**
 * Class containing the static implementation of the functions.
 */
class ScriptExternals
{
  public:
    
    /**
     * Find a function id by name.  Returns ExtNone if this is not a valid function.
     */
    static ScriptExternalDefinition* find(juce::String name);

    /**
     * Call a library function from a script.
     */
    static bool doAction(class MslContext* c, class MslAction* action);

  private:

    //
    // Function Implementations
    //

    static bool GetMidiDeviceId(class MslContext* c, class MslAction* action);
    static bool MidiOut(class MslContext* c, class MslAction* action);
    static bool assembleMidiMessage(class MslContext* c, class MslAction* action,
                                    juce::MidiMessage& msg,
                                    bool* returnSync, int *returnDeviceId);
    static int getMidiDeviceId(class MslContext* c, const char* name);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


    
