/**
 * Objects that describe errors encountered during parsing and evaluation
 * of an MSL file.
 *
 * These are maintained by MslEnvironemnt after each load for display
 * in the script diagnostic panel.
 */

#pragma once

/** 
 * Represents a single error found in a string of MSL text.
 * Usually this came from a file, but it could have be entered in the console.
 *
 * The error has the line and column numbers within the source,
 * the token string where the error was detected, and details about
 * the error left by the parser.
 */
class MslError
{
  public:

    MslError() {}
    MslError(int l, int c, juce::String t, juce::String d) {
        line = l; column = c; token = t; details = d;
    }
    ~MslError() {}

    int line = 0;
    int column = 0;
    juce::String token;
    juce::String details;

    // stupid utility to transfer from one owned array to another
    // there has to be a better way to do this
    static void transfer(juce::OwnedArray<MslError>* src,
                         juce::OwnedArray<MslError>& dest) {

        while (src->size() > 0) {
            dest.add(src->removeAndReturn(0));
        }
    }
    
};

/**
 * Represents all of the errors encountered while parsing one file.
 * The way the parser works now, there will only be one error, but that
 * may change and it might include errors detected during evaluation someday.
 *
 * Beyond the MslSerror details, this also holds the path of the file
 * and the source code of the script for display in the debug panel.
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
