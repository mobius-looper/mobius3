/**
 * A small collection of functions that can be called from scripts
 * but are not implemented as scripts or part of the MslNode model.
 */

#pragma once

typedef enum {

    MslFuncNone,
    MslFuncRand,
    MslFuncTime
    
} MslLibraryId;

class MslLibraryDefinition
{
  public:
    const char* name;
    MslLibraryId id;
};

extern MslLibraryDefinition MslLibraryDefinitions[];

class MslStandardLibrary
{
  public:
    
    /**
     * Find a definition by name.
     */
    static MslLibraryDefinition* find(juce::String name);
    
    static class MslValue* call(class MslEnvironment* env, MslLibraryId id, class MslValue* arguments);


  private:

    static class MslValue* Rand(class MslEnvironment* env, class MslValue* arguments);
    static class MslValue* Time(class MslEnvironment* env, class MslValue* arguments);
    static bool RandSeeded;
    
};

    

