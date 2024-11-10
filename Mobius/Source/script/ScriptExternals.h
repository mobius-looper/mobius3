/**
 * Implementataions of external symbols provided only for the script environment.
 * Most externals correspond go Mobius core functions that are visible in bindings.
 * These are support functions just for script authors.
 */

#pragma once

typedef enum {

    ExtNone,
    ExtMidiOut,
    ExtMax

} ScriptExternalId;

/**
 * Structure that associate a ScriptExternalId with it's name.
 */
class ScriptExternalDefinition
{
  public:
    const char* name;
    ScriptExternalId id;

    static void dump();
};

extern ScriptExternalDefinition ScriptExternalDefinitions[];

class ScriptExternals
{
  public:
    
    static ScriptExternalId find(juce::String name);

    static bool doAction(class MslContext* c, class MslAction* action);
    
  private:

    static bool MidiOut(class MslContext* c, class MslAction* action);
};


    
