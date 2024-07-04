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
    MslNode() {}
    virtual ~MslNode() {}

    virtual bool isOperator() {return false;}
    virtual bool isSymbol() {return false;}

    void add(MslNode* n) {
        n->parent = this;
        children.add(n);
    }

    void remove(MslNode* n) {
        n->parent = nullptr;
        children.removeObject(n, false);
    }

    MslNode* parent;
    juce::String bracket;

    // the mechanism for runtime evaluator dispatching
    virtual juce::String eval(class MslEvaluator* ev) = 0;

    // would like to protect this...
    juce::OwnedArray<MslNode> children;

  protected:
    
};

// see if we can get away without this
// if nodes have all the blockness we need then the only
// purpose this serves is as an anonymous container for the root
// then we could just give our eval() to MslNode

class MslBlock : public MslNode
{
  public:
    MslBlock() {}
    virtual ~MslBlock() {}
    juce::String eval(class MslEvaluator* ev) {
        return ev->evalBlock(this);
    }
};

class MslSymbol : public MslNode
{
  public:
    MslSymbol(juce::String s) {name = s;}
    virtual ~MslSymbol() {}

    bool isSymbol() override {return true;}

    juce::String name;
    class Symbol* symbol = nullptr;

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
    MslLiteral(juce::String s) {value = s;}
    MslLiteral(juce::String s, int ival) {(void)ival; value = s; isInt=true;}
    MslLiteral(juce::String s, float fval) {(void)fval; value = s; isFloat=true;}
    MslLiteral(juce::String s, bool bval) {(void)bval; value = s; isBool=true;}
    virtual ~MslLiteral() {}

    bool isBool = false;
    bool isInt = false;
    bool isFloat = false;

    // we used the constructor arg values to derive the type flags
    // don't really need to save the coerced value, a little weird
    
    juce::String value;

    juce::String eval(class MslEvaluator* ev) {
        return ev->evalLiteral(this);
    }
    
};

class MslOperator : public MslNode
{
  public:
    MslOperator(juce::String s) {op = s;}
    virtual ~MslOperator() {}
    
    bool isOperator() override {return true;}

    bool isComplete() {
        // todo: unary not supported
        return (children.size() == 2);
    }

    juce::String op;

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

