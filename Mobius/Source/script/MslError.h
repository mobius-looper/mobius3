/**
 * Object that describe errors encountered during parsing and evaluation
 * of an MSL file.
 *
 * This started life using juce::String which is fine for the parser but
 * not for the interpreter because of memory allocation restrictions.  So
 * it is a pooled object with static string arrays that need to be long
 * enough for most errors.
 */

#pragma once

/** 
 * Represents a single error found in a string of MSL text.
 * The error has the line and column numbers within the source,
 * the token string where the error was detected, and details about
 * the error left by the parser or interpreter.
 *
 * This object is part of the MslPools model and is not allowed
 * to use anything that would result in dynamic memory allocation.
 */
class MslError
{
  public:

    MslError();
    MslError(MslError* src);
    // constructor used by the parser which likes juce::String
    MslError(int l, int c, juce::String t, juce::String d);
    // constructor used by the linker
    MslError(class MslNode* node, juce::String d);
    ~MslError();

    // initializer for the object pool
    void init();
    // initializer used by MslSession interpreter which
    // uses pooled objects and string buffers
    void init(class MslNode* node, const char* details);

    int line = 0;
    int column = 0;

    static const int MslMaxErrorToken = 64;
    char token[MslMaxErrorToken];
    void setToken(const char* src);

    static const int MslMaxErrorDetails = 128;
    char details[MslMaxErrorDetails];
    void setDetails(const char* src);

    // chain pointer when used in the kernel
    class MslError* next = nullptr;

    // for older shell level classes that need to transfer OwnedArrays
    static void transfer(juce::OwnedArray<MslError>* src, juce::OwnedArray<MslError>* dest) {
        while (src->size() > 0) {
            dest->add(src->removeAndReturn(0));
        }
    }
    
};
