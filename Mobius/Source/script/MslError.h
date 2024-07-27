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
    // constructor used by the parser which likes juce::String
    MslError(int l, int c, juce::String t, juce::String d);
    ~MslError();

    // initializer for the object pool
    void init();
    // initializer used by MslSession interpreter
    void init(MslNode* node, const char* details);

    int line = 0;
    int column = 0;

    static const int MslMaxErrorToken = 64;
    char token[MslMaxErrorToken];

    static const int MslMaxErrorDetails = 128;
    char details[MslMaxErrorDetails];

    // chain pointer when used in the kernel
    MslError* next = nullptr;
    
};

/**
 * Represents all of the errors encountered while parsing one file.
 * The way the parser works now, there will only be one error, but that
 * may change and it might include errors detected during evaluation someday.
 *
 * Beyond the MslSerror details, this also holds the path of the file
 * and the source code of the script for display in the debug panel.
 *
 * Since the parser resides only in shell threads this object is NOT
 * pooled and may make use of juce::String
 */
class MslFileErrors
{
  public:

    MslFileErrors() {}
    ~MslFileErrors() {}

    // file this came from
    juce::String path;

    // source code that was read
    juce::String code;

    // errors encountered
    // todo: since MslError is simple enough, this could just be an Array of objects
    // todo: might still want to just have a linked list here for consistency with
    // MslSession.  Need a conversion utility
    juce::OwnedArray<MslError> errors;

};

/**
 * Represents a collision between reference name symbols.  The collision
 * may be resolved by renaming the script or export, or by unloading one
 * of the other scripts.
 */
class MslCollision
{
  public:
    MslCollision() {}
    ~MslCollision() {}

    // the name that is in conflict
    juce::String name;

    // the script that wanted to install the duplicadte name
    juce::String fromPath;

    // the script that already claimed this name
    juce::String otherPath;

    // todo remember whether this was a collision on the outer script
    // name or an exported proc/var inside it
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
