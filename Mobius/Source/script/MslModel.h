/**
 * Parse tree model for MSL scripts
 */

#pragma once

#include <JuceHeader.h>
#include "MslTokenizer.h"

class MslVisitor
{
  public:
    virtual ~MslVisitor() {}

    virtual void mslVisit(class MslLiteral* obj) = 0;
    virtual void mslVisit(class MslSymbol* obj) = 0;
    virtual void mslVisit(class MslBlock* obj) = 0;
    virtual void mslVisit(class MslOperator* obj) = 0;
    virtual void mslVisit(class MslAssignment* obj) = 0;
    virtual void mslVisit(class MslVar* obj) = 0;
    virtual void mslVisit(class MslProc* obj) = 0;
};

class MslNode
{
  public:
    MslNode(juce::String t) {token = t;}
    virtual ~MslNode() {}

    juce::String token;
    MslNode* parent;
    // would like to protect this, but we've got the ownership issue
    juce::OwnedArray<MslNode> children;

    //////////////////////////////////////////////////////////////////////
    // Parse State
    //////////////////////////////////////////////////////////////////////
    
    // due to the weird way we Symbols consume sibling () blocks
    // and the way Operator swaps our location and puts it
    // under a new block, we need to prevent assimulation of
    // any future blocks, could proabbly handle this in the parser
    // but it's easy enough here
    bool locked = false;
    
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

    // returns true if the node would like to consume the token
    virtual bool wantsToken(MslToken& t) {(void)t; return false;}

    // returns true if the node would like to consume another child node
    // do we really need locked if this works?
    virtual bool wantsNode(MslNode* node) {(void)node; return false;}
    
    //////////////////////////////////////////////////////////////////////
    // Runtime State
    //////////////////////////////////////////////////////////////////////
    
    bool hasBlock(juce::String bracket) {
        bool found = false;
        for (auto child : children) {
            if (child->token == bracket) {
                found = true;
                break;
            }
        }
        return found;
    }

    virtual bool isLiteral() {return false;}
    virtual bool isSymbol() {return false;}
    virtual bool isBlock() {return false;}
    virtual bool isOperator() {return false;}
    virtual bool isAssignment() {return false;}
    virtual bool isVar() {return false;}
    virtual bool isProc() {return false;}

    virtual void visit(MslVisitor* visitor) = 0;

    // console tools

    void detach() {
        if (parent != nullptr) {
            parent->remove(this);
        }
    }
    
};

class MslLiteral : public MslNode
{
  public:
    MslLiteral(juce::String s) : MslNode(s) {locked=true;}
    virtual ~MslLiteral() {}

    bool isLiteral() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

    // could use an MslValue here, but we've already
    // stored the string in token and I don't want
    // to drag in MslTokenizer::Token?
    // hmm...
    bool isBool = false;
    bool isInt = false;
    bool isFloat = false;
};

class MslBlock : public MslNode
{
  public:
    MslBlock(juce::String token) : MslNode(token) {}
    virtual ~MslBlock() {}

    // doesn't want tokens but will always accept nodes
    // might want tokens if inner blocks allow declarations
    // or are those handled as preprocessor lines?
    // I guess this is where locking comes in to play
    // unless we consume the close bracket token and remember that
    // to make wantsNode return false, this will always hapily take nodes
    bool wantsNode(MslNode* node) override {(void)node; return true;}

    bool isBlock() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
};

class MslSymbol : public MslNode
{
  public:
    MslSymbol(juce::String s) : MslNode(s) {}
    virtual ~MslSymbol() {}

    // symbols only allow () arguemnt blocks, which turns them into
    // a parameterized reference, aka a "call"
    // originally I allowed them to accept {} body blocks and magically
    // become a proc, but I think no, require a proc keyword

    bool wantsNode(MslNode* node) override {
        bool wants = false;
        if (node->token == "(" && children.size() == 0)
          wants = true;
        return wants;
    }
    
    // runtime state
    class Symbol* symbol = nullptr;
    
    bool isSymbol() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}


};

class MslOperator : public MslNode
{
  public:
    MslOperator(juce::String s) : MslNode(s) {}
    virtual ~MslOperator() {}

    // operators stop accepting nodes with all of their operands
    // are satisified
    // todo: need to support unary
    // disallow structural nodes like proc and var
    bool wantsNode(MslNode* node) override {
        bool wants = false;
        if (children.size() < 2 &&
            (node->isLiteral() || node->isSymbol() || node->isOperator() ||
            // for blocks, should only see ()
            // I guess we can allow {} under the assumption that blocks return
            // their last value, nice way to encapsulate a multi-step computation
            // that actually gets you C-style ? operators
             node->isBlock() ||
             // what about Assignment?  it would be unusual to have one of those inside
             // an expression, what does C do?
             // the value of an assignment, is the assigned value
             // this will look confusing though since = is often misused as ==
             node->isAssignment()))
          wants = true;

        return wants;
    }

    // if we rejected a node and our operands are not satisified, it is usually
    // a syntax error like "x + proc"
    // unclear if we want to halt when that happens, or just let it dangle
    // should warn at runtime, to catch that early, will need a lock() method
    // that tests the lockability of the node

    // is this necessary?
    bool unary = false;

    // runtime
    bool isOperator() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

};

// assignments are basically operators with added runtime semantics
class MslAssignment : public MslNode
{
  public:
    MslAssignment(juce::String s) : MslNode(s) {}
    virtual ~MslAssignment() {}

    bool wantsNode(MslNode* node) override {
        bool wants = false;
        if (children.size() < 2 &&
            (node->isLiteral() || node->isSymbol() || node->isOperator() ||
             node->isBlock() ||
             node->isAssignment()))
          wants = true;
        return wants;
    }

    bool isAssignment() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

};    
    
class MslVar : public MslNode
{
  public:
    MslVar(juce::String token) : MslNode(token) {}
    virtual ~MslVar() {}

    // var is one of the few that consumes tokens
    // hmm, it's a little more than this, it REQUIRES a token
    // wantsToken doesn't have a way to reject with prejeduce
    // we'll end up with a bad parse tree that will have to be caught at runtime
    bool wantsToken(MslToken& t) override {
        bool wants = false;
        if (t.type == MslToken::Type::Symbol) {
            // take this as our name
            name =  t.value;
            wants = true;
        }
        return wants;
    }

    // vars accept an espression
    bool wantsNode(MslNode* node) override {
        // okay this is the same as Operator and Assignment
        // except we only accept one child
        // need an isExpression() that encapsulates this
        bool wants = false;
        if (children.size() < 1 &&
            (node->isLiteral() || node->isSymbol() || node->isOperator() ||
             node->isBlock()))
          wants = true;
        return wants;
    }

    juce::String name;

    bool isVar() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
};

class MslProc : public MslNode
{
  public:
    MslProc(juce::String token) : MslNode(token) {}
    virtual ~MslProc() {}

    // same as var
    bool wantsToken(MslToken& t) override {
        bool wants = false;
        if (t.type == MslToken::Type::Symbol) {
            name =  t.value;
            wants = true;
        }
        return wants;
    }

    bool wantsNode(MslNode* node) override {
        bool wants = false;
        if (!hasArgs && node->isBlock() && node->token == "(") {
            hasArgs = true;
            wants = true;
        }
        else if (!hasBody && node->isBlock() && node->token == "{") {
            hasBody = true;
            wants = true;
        }
        return wants;
    }

    juce::String name;
    bool hasArgs = false;
    bool hasBody = false;
    
    bool isProc() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
};

#if 0


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

