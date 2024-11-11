/**
 * Implementataions of external symbols provided only for the script environment.
 * Most externals correspond go Mobius core functions that are visible in bindings.
 * These are support functions just for script authors.
 */

#pragma once

#include <JuceHeader.h>

/**
 * Internal ids for the built-in external functions.
 * These are what is placed in the MslExternal when resolving
 * a reference from a script.
 */
typedef enum {

    ExtNone,
    ExtGetMidiDeviceId,
    ExtMidiOut,
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
 * Structure that associate a ScriptExternalId with it's name.
 */
class ScriptExternalDefinition
{
  public:
    const char* name;
    ScriptExternalId id;
    ScriptContext context;

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
    static ScriptExternalId find(juce::String name);

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
    static bool assembleMidiMessage(class MslAction* action, juce::MidiMessage& msg,
                                    bool* returnSync, int *returnDeviceId);
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/


    
