/**
 * Outer package for a compiled script.
 *
 */

#pragma once

class MslScript {
  public:
    MslScript() {}
    ~MslScript() {}

    /**
     * The file this came from if loaded from a file.
     * If this is empty, it is a scriptlet.
     */
    juce::String filename;

    /**
     * The root block.  Evaluation of a script normally begins with the
     * first node in this block, but may start at any exported proc block.
     */
    MslBlock* root = nullptr;

    // todo: capture any ! directivies found during parsing
    // with the new tokenizer, ! is an operator and # is used for preprocessor
    // comamnds line C so it may be better to switch to # for these
    // makes them look more like C programs than shell scripts

    // todo: maintain a list of all exported procs
    // I'd rather use procs than labels for sustain, multiclick, etc.

    // todo: a list of exported vars

    // todo: does it make any sense to keep a copy of the source code here?
    // might be useful if we ever get around to showing where errors are in the code

    // a list of error messages generated during parsing and linking
    juce::OwnedArray<class MslError> errors;
};

class MslError {
  public:
    MslError() {}
    ~MslError() {}

    juce::String message;
    int line = 0;
    int column = 0;

    // todo: distinguish between errors, warnings, and information
    // maybe keep those on different lists?
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
    
    
  
