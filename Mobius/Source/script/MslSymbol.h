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

    void link(class MslContext* context, class MslEnvironment* env,
              class MslResolutionContext* rc, class MslCompilation* comp) override;
    
    // link state
    // if a name resolves to more than one thing, only one of them will be set

    // things resolved within the MslCompilation (i.e. in the same script)
    // think about this...it would be easier in some ways to have non interned
    // linkages in the MslCompilation so we could represent these the same way
    class MslFunctionNode* function = nullptr;
    class MslVariable* variable = nullptr;

    // reference resolved within the environment or resolution context
    class MslLinkage* linkage = nullptr;

    // reference resolved to an external
    // same thing with external that have a signature
    // factor out an MslCallable that has all these things
    class MslExternal* external = nullptr;

    // compiled argument list for the resolved function
    MslBlock arguments;
    
  private:

    void linkCall(class MslScript* script, class MslBlock* signature);
    class MslAssignment* findCallKeyword(juce::Array<class MslNode*>& callargs, juce::String name);
    class MslNode* findCallPositional(juce::Array<class MslNode*>& callargs);
    void linkAssignment(class MslContext* context, class MslEnvironment* env, class MslScript* script);

};

