/**
 * Implementataions of external symbols provided only for the script environment.
 * Most externals correspond go Mobius core functions that are visible in bindings.
 * These are support functions just for script authors.
 */

#pragma once

#include <JuceHeader.h>

#include "ScriptExternalId.h"
#include "MslValue.h"

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

    static bool InstallUIElement(class MslContext* c, class MslAction* action);
    static bool buildMap(class MslValue* plist, juce::HashMap<juce::String,juce::String>& map);

};

/**
 * Experimental...
 */
class VarQuery {
  public:
    ScriptExternalId id = ExtNone;
    int scope = 0;
    MslValue result;
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
