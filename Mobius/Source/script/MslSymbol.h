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
#include "MslLinkage.h"
#include "MslExternal.h"
#include "MslFunction.h"
#include "MslStandardLibrary.h"

//////////////////////////////////////////////////////////////////////
//
// Resolution
//
//////////////////////////////////////////////////////////////////////

/**
 * Helper class to deal with all the various resolution targets
 * when resolving symbols.  One of these is embedded within each MslSymbolNode
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

    // a local variable at any level of scope under the root block
    // this is by far the most common
    // todo: we don't really need this pointer, the value will be stored
    // as an MslBinding on the stack where it will resolve, all we need to remember
    // here is a flag saying it did resolve to a variable node
    class MslVariableNode* innerVariable = nullptr;

    // a top-level static (global, public, exported) script variable
    // this will also have a VariableNode in the tree, but the value will be stored
    // here rather than the stack so it is visible to other scripts if it is public
    class MslVariable* staticVariable = nullptr;

    // a function argument declared within the containing function definition
    bool functionArgument = false;

    // an "inner" function definition
    // these are not fully supported yet but prepare for it
    class MslFunctionNode* innerFunction = nullptr;

    // a top-level local function, these are common
    class MslFunction* rootFunction = nullptr;

    // a link to an exported function or variable from another script
    class MslLinkage* linkage = nullptr;

    // an external function or variable defined by the containing application
    class MslExternal* external = nullptr;

    // a standard library function id
    MslLibraryId internal = MslFuncNone;

    // an external keyword defined by the containing application
    bool keyword = false;

    // an external usage argument
    // could lwe just use keyword for this too?
    bool usageArgument = false;

    // a "carryover" variable defined in a prior scriptlet session
    // hate this, do we actually need all these flags?
    // a single "internal" to prevent unresolved errors should be enough
    // actually this should just be a staticVariable now, get rid of this
    bool carryover = false;

    void reset() {
        innerVariable = nullptr;
        staticVariable = nullptr;
        functionArgument = false;
        innerFunction = nullptr;
        rootFunction = nullptr;
        linkage = nullptr;
        external = nullptr;
        internal = MslFuncNone;
        keyword = false;
        usageArgument = false;
        carryover = false;
    }

    // true if we found something
    bool isResolved() {
        return (innerVariable != nullptr ||
                staticVariable != nullptr ||
                functionArgument ||
                innerFunction != nullptr ||
                rootFunction != nullptr ||
                linkage != nullptr ||
                external != nullptr ||
                internal != MslFuncNone ||
                keyword || usageArgument || carryover);
    }

    // true if what we found is a function
    bool isFunction() {
        return (innerFunction != nullptr ||
                rootFunction != nullptr ||
                internal != MslFuncNone ||
                (linkage != nullptr && linkage->function != nullptr) ||
                (external != nullptr && external->isFunction));
    }

    // return the function body to evaluate from wherever it may roam
    MslBlockNode* getBody() {
        MslBlockNode* body = nullptr;
        
        if (innerFunction != nullptr) {
            body = innerFunction->getBody();
        }
        else if (rootFunction != nullptr) {
            body = rootFunction->getBody();
        }
        else if (linkage != nullptr) {
            if (linkage->function != nullptr)
              body = linkage->function->getBody();
        }
        return body;
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
    
    MslArgumentNode* getArgument() override {return this;}
    bool operandable() override {return false;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Argument";}

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
class MslSymbolNode : public MslNode
{
  public:
    
    MslSymbolNode(MslToken& t) : MslNode(t) {}
    virtual ~MslSymbolNode() {}

    MslSymbolNode* getSymbol() override {return this;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Symbol";}

    // symbols only allow () arguemnt blocks, which turns them into
    // a parameterized reference, aka a "call"
    // originally I allowed them to accept {} body blocks and magically
    // become a proc, but I think no, require a proc keyword

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p;
        bool wants = false;
        if (node->token.value == "(" && children.size() == 0)
          wants = true;
        return wants;
    }

    // link state
    MslResolution resolution;
    bool isResolved() {return resolution.isResolved();}
    
    // compiled argument list for the resolved function
    MslBlockNode arguments;
    
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
