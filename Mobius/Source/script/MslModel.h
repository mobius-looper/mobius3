/**
 * Parse tree model for MSL scripts
 */

#pragma once

#include <JuceHeader.h>

// have to include this if we want to be a header-only class
// unfortunate because this may drag in a bunch of stuff
#include "MslEvaluator.h"

class MslNode
{
  public:
    MslNode(juce::String t) {token = t;}
    virtual ~MslNode() {}

    juce::String token;
    MslNode* parent;

    void add(MslNode* n) {
        n->parent = this;
        children.add(n);
    }

    void remove(MslNode* n) {
        n->parent = nullptr;
        children.removeObject(n, false);
    }

    MslNode* getLast() {
        return children.getLast();
    }

    // would like to protect this, but we've got the ownership issue
    juce::OwnedArray<MslNode> children;

    virtual bool isBlock() {return false;}
    virtual bool isSymbol() {return false;}
    virtual bool isLiteral() {return false;}
    virtual bool isOperator() {return false;}

    // the mechanism for runtime evaluator dispatching
    virtual juce::String eval(class MslEvaluator* ev) = 0;

  protected:
    
};

// see if we can get away without this
// if nodes have all the blockness we need then the only
// purpose this serves is as an anonymous container for the root
// then we could just give our eval() to MslNode

class MslBlock : public MslNode
{
  public:
    MslBlock(juce::String token) : MslNode(token) {}
    virtual ~MslBlock() {}

    bool isBlock() override {return true;}
    juce::String eval(class MslEvaluator* ev) {
        return ev->evalBlock(this);
    }
};

class MslSymbol : public MslNode
{
  public:
    MslSymbol(juce::String s) : MslNode(s) {}
    virtual ~MslSymbol() {delete arguments;}

    class Symbol* symbol = nullptr;
    // should be smart, or better inline
    MslBlock* arguments = nullptr;;

    bool isSymbol() override {return true;}
    juce::String eval(class MslEvaluator* ev) {
        return ev->evalSymbol(this);
    }
};

/**
 * Still working out how to represent these.
 * We need the tokenizer type, but I'm not sure I want to make
 * this depend on the Tokenizer model we just happen to be using
 * at the moment.  Yet another type enumeration is annoying.
 * We don't have many of these so try flags.  It actually looks
 * cleaner to do "if (node.isBool)" than "if (node.type == AnotherTypeEnum::Bool)"
 */
class MslLiteral : public MslNode
{
  public:
    MslLiteral(juce::String s) : MslNode(s) {}
    virtual ~MslLiteral() {}

    bool isBool = false;
    bool isInt = false;
    bool isFloat = false;
    
    bool isLiteral() override {return true;}
    juce::String eval(class MslEvaluator* ev) {
        return ev->evalLiteral(this);
    }
};

class MslOperator : public MslNode
{
  public:
    MslOperator(juce::String s) : MslNode(s) {}
    virtual ~MslOperator() {}

    bool isComplete() {
        // todo: unary not supported
        return (children.size() == 2);
    }

    bool isOperator() override {return true;}
    juce::String eval(class MslEvaluator* ev) {
        return ev->evalOperator(this);
    }
    
};

#if 0
class MslProc : public MslNode
{
  public:
    MslProc() {}
    ~MslProc() {}

    juce::String name;
    
};

class MslVar : public MslNode
{
  public:
    MslVar() {}
    ~MslVar() {}

    juce::String name;
    
};

class MslFunction : public MslNode
{
  public:
    MslFunction() {}
    ~MslFunction() {}

    class Symbol* symbol = nullptr;
};

class MslIf : public MslNode
{
  public:
    MslIf() {}
    ~MslIf() {}

    MslNode trueBlock;
    MslNode falseBlock;
};
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

