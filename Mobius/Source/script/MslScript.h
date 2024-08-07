/**
 * Outer package for a compiled script.
 *
 * The object contains a mixture of fixed structure that resulted
 * from parsing the script language, and runtime structure that is built
 * as the script is used.
 */

#pragma once

class MslScript {
  public:
    MslScript();
    ~MslScript();

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
     * The root block.  MslNode objects representing the top-level statements
     * parsed from the file will be placed inside the root block.
     * Evaluation of the file normally begins with the first node in this
     * block, but may alternately start at any exported proc block,
     * or Label.
     */
    // use std::unique_ptr here...
    class MslBlock* root = nullptr;

    // proc definitions found within the script source
    // these are gathered here rather than left within the root block
    // to make them easier to find when referenced
    // todo: may want the distinction between top-level and block-scoped
    // procs.
    // todo: OwnedArray is not good for runtime memory allocation
    // better to just leave them in place and cache them in the referencing
    // MslSymbol node after they're found?
    // this is however functioning as a history of prior definitions
    // when using the console and scriptlet sessions.  I'm starting
    // to not like doing that here since it is very specific to the console
    juce::OwnedArray<class MslProc> procs;

    // runtime cache of static variable bindings
    // this uses a pooled object linked list rather than OwnedArray
    // to avoid memory at runtime and be like MslStack
    // think about a smart pointer variant that can deal with pooled objects
    class MslBinding* bindings = nullptr;
    
    // todo: capture any directives (like !button) found during parsing

    // todo: support the old concept of Labels ?
    // I think no, use exported procs instead

    class MslProc* findProc(juce::String procname);

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

    
    
    
  
