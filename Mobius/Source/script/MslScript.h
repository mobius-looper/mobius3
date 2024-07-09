/**
 * Outer package for a compiled script.
 *
 */

#pragma once

#include "MslModel.h"

class MslError {
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
 
    // todo: distinguish between errors, warnings, and information
    // maybe keep those on different lists?
};

class MslScript {
  public:
    MslScript() {}
    ~MslScript() {delete root;}

    /**
     * The file this came from if loaded from a file.
     * If this is empty, it is a scriptlet.
     */
    juce::String filename;

    /**
     * The root block.  Evaluation of a script normally begins with the
     * first node in this block, but may start at any exported proc block.
     */
    class MslBlock* root = nullptr;

    // proc definitions found within the script source
    // these are gathered here rather than left within the root block
    // todo: will want a distinction between local and exported procs
    // that can be referenced from the outside
    juce::OwnedArray<class MslProc> procs;

    // var definitions found within the script source
    // todo: work out how symbol overrides work, are all vars at any level
    // promoted to be accessible vars for the entire script, or do they
    // have block scope?
    // todo: will want a distinction between local and exported vars
    // that can be referenced from the outside
    juce::OwnedArray<class MslVar> vars;

    // todo: capture any directives (like !button) found during parsing

    // todo: does it make any sense to keep a copy of the source code here?
    // might be useful if we ever get around to showing where errors are in the code

    // todo: support the old concept of Labels ?
    // I think no, use exported procs instead

    // a list of error messages encountered during parsing and linking
    juce::Array<class MslError> errors;

    class MslProc* findProc(juce::String name) {
        MslProc* found = nullptr;
        for (auto proc : procs) {
            if (proc->name == name) {
                found = proc;
                break;
            }
        }
        return found;
    }

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
    
    
  
