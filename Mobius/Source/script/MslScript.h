/**
 * Outer package for a compiled script.
 *
 */

#pragma once

#include "MslModel.h"

class MslScript {
  public:
    MslScript() {}
    ~MslScript() {delete root;}

    /**
     * The file this came from if loaded from a file.
     * If this is empty, it is a scriptlet.
     */
    juce::String path;

    /**
     * The reference name for this script.  This is taken from the filename
     * unless the #name directive is found in the script source
     */
    juce::String name;

    /**
     * The root block.  Evaluation of a script normally begins with the
     * first node in this block, but may start at any exported proc block.
     */
    // use std::unique_ptr here...
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

    // todo: support the old concept of Labels ?
    // I think no, use exported procs instead

    class MslProc* findProc(juce::String procname) {
        MslProc* found = nullptr;
        for (auto proc : procs) {
            if (proc->name == procname) {
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

    
    
    
  
