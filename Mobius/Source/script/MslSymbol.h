/**
 * Implementation related to Symbol nodes.
 *
 * The symbol node is more complex than most as it requires linking.
 * Linking first attempts to resolve the symbol name to an an MslFunction or
 * MslVariable within the MslEnvironemnt.  If not found there, it will ask the
 * context if there is an MslExternal with that name.
 *
 * Once the symbol has been resolved, if this represents a function call,
 * 
 */

#pragma once

#include <JuceHeader.h>

#include "MslModel.h"

//////////////////////////////////////////////////////////////////////
//
// Resolution
//
//////////////////////////////////////////////////////////////////////

/**
 * Helper class to deal with all the various resolution targets
 * when resolving symbols.  One of these is embedded within each MslSymbol
 * Besides maintaining state while searching for things during linking it also
 * hides the details of the various object models at runtime to simplify code
 * in the interpreter.
 *
 * This is kind of a mess right now so I'm encapsulating as much of it as
 * possible here so we can tinker with the model without disrupting the
 * linker and interpreter.
 *
 */
class MslResolution
{
  public:

    MslResolution() {}
    ~MslResolution() {}

    // a local variable defined somewhere in the tree
    // this is by far the most common
    class MslVariable* localVariable = nullptr;

    // an "inner" function definition
    // these are not fully supported yet but prepare for it
    class MslFunctionNode* innerFunction = nullptr;

    // a top-level local function, these are common
    class MslFunction* localFunction = nullptr;

    // a link to an exported function or variable from another script
    class MslLinkage* linkage = nullptr;

    // an external function or variable defined by the contraining application
    class MslExternal* external = nullptr;

    void reset() {
        localVariable = nullptr;
        innerFunction = nullptr;
        localFunction = nullptr;
        linkage = nullptr;
        externa = nullptr;
    }

    // true if we found something
    bool isResolved() {
        return (localVariable != nullptr ||
                innerFunction != nullptr ||
                localFunction != nullptr ||
                linkage != nullptr ||
                external != nullptr);
    }

    // true if what we found is a function
    bool isFunction() {
        (innerFunction != nullptr ||
         localFunction != nullptr ||
         (linkage != nullptr && linkage->function != nullptr) ||
         (external != nullptr && external->isFunction));
    }
    
};

//////////////////////////////////////////////////////////////////////
//
// Call Arguments
//
//////////////////////////////////////////////////////////////////////

/**
 * Call argument
 * These are not pooled, they are constructured at link time.
 */
class MslArgumentNode : public MslNode
{
  public:
    MslArgumentNode() : MslNode() {}
    virtual ~MslArgumentNode() {}
    
    bool isArgument() override {return true;}
    bool operandable() override {return false;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

    // the name of the argument we are satisfying
    juce::String name;

    // the position of this argument, necessary?
    int position = 0;

    // true if this is an extra call argument that didn't match
    // an argument in the function declaration
    bool extra = false;

    // true if this was after an :optional keyword and doesn't
    // require a value
    bool optional = false;
    
    // the thing we forward to
    MslNode* node = nullptr;

};

//////////////////////////////////////////////////////////////////////
//
// Symbol
//
//////////////////////////////////////////////////////////////////////

/**
 * Symbol node
 */
class MslSymbol : public MslNode
{
  public:
    
    MslSymbol(MslToken& t) : MslNode(t) {}
    virtual ~MslSymbol() {}

    bool isSymbol() override {return true;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

    // symbols only allow () arguemnt blocks, which turns them into
    // a parameterized reference, aka a "call"
    // originally I allowed them to accept {} body blocks and magically
    // become a proc, but I think no, require a proc keyword

    bool wantsNode(MslNode* node) override {
        bool wants = false;
        if (node->token.value == "(" && children.size() == 0)
          wants = true;
        return wants;
    }

    // link state
    MslResolution resolution;
    bool isResolved() {resolution.isResolved();}
    
    // compiled argument list for the resolved function
    MslBlock arguments;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
