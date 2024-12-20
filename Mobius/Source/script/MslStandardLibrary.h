/**
 * A small collection of functions that can be called from scripts
 * but are not implemented as scripts or part of the MslNode model.
 */

#pragma once

typedef enum {

    MslFuncNone,
    MslFuncEndSustain,
    MslFuncEndRepeat,
    MslFuncRand,
    MslFuncTime,
    MslFuncSampleRate,
    MslFuncTempo
    
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
    
    static class MslValue* call(class MslSession* s, MslLibraryId id, class MslValue* arguments);

  private:

    static class MslValue* EndSustain(class MslSession* s, class MslValue* arguments);
    static class MslValue* EndRepeat(class MslSession* s, class MslValue* arguments);
    static class MslValue* Rand(class MslSession* s, class MslValue* arguments);
    static class MslValue* Time(class MslSession* s, class MslValue* arguments);
    static class MslValue* SampleRate(class MslSession* s, class MslValue* arguments);
    static class MslValue* Tempo(class MslSession* s, class MslValue* arguments);
    static bool RandSeeded;
    
};

    

