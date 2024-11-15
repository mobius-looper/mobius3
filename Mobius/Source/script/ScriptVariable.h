/**
 * Definitions for external variables that can be referenced from scripts.
 *
 * These are similar to Symbols, except they are not writable and are not
 * a persistent part of the configuration like parameters.  They provide
 * access to random bits of runtime state that is interesting for script
 * writers.
 *
 * It is conceptually the same as what old Mobius scripts call ScriptInternalVariables.
 * There is support down in Kernel to access those, but I want to migrate away
 * from a dependency on those for MSL.
 *
 * Like Symbol, these fundamentally have a unique identifier and a reference name.
 * Unlike Symbol they don't have any formal definition object, there is just code
 * that switches on the id to determine the current value.
 *
 */

#pragma once

typedef enum {

    VarNone,

    VarTrackNumber,

    VariableIdMax

} ScriptVariableId;

class ScriptVariableDefinition
{
  public:
    const char* name;
    ScriptVariableId id;

    static ScriptVariableId find(const char* name);

};

extern ScriptVariableDefinition ScriptVariableDefinitions[];

class ScriptVariableHandler
{
  public:

    static void get(class MslContext* c, ScriptVariableId id, MslValue* value);
};

    



