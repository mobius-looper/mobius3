/**
 * Parse tree model for MSL scripts
 *
 * This is starting to have an awkward mixture of parse state and
 * runtime state.  Starting to think it would be better to separate the two
 * with MslNode being transient parse state that is post-processed aka "linked"
 * to produce an different model for evaluation.  This makes the separation between
 * the parser and the evaluator cleaner, which is better because I'm not liking
 * how hacky the parser is.
 *
 */

#pragma once

#include <JuceHeader.h>
#include "MslTokenizer.h"
#include "MslWait.h"

//////////////////////////////////////////////////////////////////////
//
// Visitor
//
//////////////////////////////////////////////////////////////////////

/**
 * An interface to be implemented by something that wants to walk
 * over the parse tree without calling isFoo on every node.
 */
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
    virtual void mslVisit(class MslIf* obj) = 0;
    virtual void mslVisit(class MslElse* obj) = 0;
    virtual void mslVisit(class MslReference* obj) = 0;
    virtual void mslVisit(class MslEnd* obj) = 0;
    virtual void mslVisit(class MslWaitNode* obj) = 0;
    virtual void mslVisit(class MslEcho* obj) = 0;
    virtual void mslVisit(class MslContextNode* obj) = 0;
};

//////////////////////////////////////////////////////////////////////
//
// Node
//
//////////////////////////////////////////////////////////////////////

/**
 * The parse tree is a tree of node subclasses.
 * Each node has one parent and multiple children.
 * Node subclasses assist in parsing by telling the MslParser if
 * they want to accept the next token or other node.
 */
class MslNode
{
  public:
    MslNode() {}
    MslNode(MslToken& t) {token = t;}
    virtual ~MslNode() {}

    MslToken token;
    MslNode* parent = nullptr;
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

    int size() {
        return children.size();
    }

    MslNode* get(int i) {
        return ((i >= 0 && i < children.size()) ? children[i] : nullptr);
    }

    MslNode* getLast() {
        return children.getLast();
    }

    // returns true if the node would like to consume the token
    virtual bool wantsToken(class MslParser* p, MslToken& t) {(void)p; (void)t; return false;}

    // returns true if the node would like to consume another child node
    // do we really need locked if this works?
    virtual bool wantsNode(MslNode* node) {(void)node; return false;}

    // returns true if the node can behave as an operand
    // most of them except for keywoards like if/else/var
    virtual bool operandable() {return false;}
    
    //////////////////////////////////////////////////////////////////////
    // Runtime State
    //////////////////////////////////////////////////////////////////////
    
    bool hasBlock(juce::String bracket) {
        bool found = false;
        for (auto child : children) {
            if (child->token.value == bracket) {
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
    virtual bool isIf() {return false;}
    virtual bool isElse() {return false;}
    virtual bool isReference() {return false;}
    virtual bool isEnd() {return false;}
    virtual bool isWait() {return false;}
    virtual bool isEcho() {return false;}
    virtual bool isContext() {return false;}
    
    virtual void visit(MslVisitor* visitor) = 0;

    // console tools

    void detach() {
        if (parent != nullptr) {
            parent->remove(this);
        }
    }
    
};

//////////////////////////////////////////////////////////////////////
//
// Literal
//
//////////////////////////////////////////////////////////////////////

class MslLiteral : public MslNode
{
  public:
    MslLiteral(MslToken& t) : MslNode(t) {locked=true;}
    virtual ~MslLiteral() {}

    bool isLiteral() override {return true;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

    // could use an MslValue here, but we've already
    // stored the string in token and I don't want
    // to drag in MslTokenizer::Token?
    // hmm...
    bool isBool = false;
    bool isInt = false;
    bool isFloat = false;
};

//////////////////////////////////////////////////////////////////////
//
// Reference
//
//////////////////////////////////////////////////////////////////////

class MslReference : public MslNode
{
  public:
    MslReference(MslToken& t) : MslNode(t) {locked=true;}
    virtual ~MslReference() {}

    // take the next number or symbol
    // if it isn't one of those raise an error
    bool wantsToken(class MslParser* p, MslToken& t) override;
    
    juce::String name;
    
    bool isReference() override {return true;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
};

//////////////////////////////////////////////////////////////////////
//
// Block
//
//////////////////////////////////////////////////////////////////////

class MslBlock : public MslNode
{
  public:
    MslBlock(MslToken& t) : MslNode(t) {}
    // special constructor for the root block with no tokcn
    MslBlock() {}
    virtual ~MslBlock() {}

    // doesn't want tokens but will always accept nodes
    // might want tokens if inner blocks allow declarations
    // or are those handled as preprocessor lines?
    // I guess this is where locking comes in to play
    // unless we consume the close bracket token and remember that
    // to make wantsNode return false, this will always hapily take nodes
    bool wantsNode(MslNode* node) override {(void)node; return true;}

    // blocks accumulate procs and vars defined within it outside of the
    // logic node tree

    juce::OwnedArray<MslProc> procs;
    juce::OwnedArray<MslVar> vars;

    bool isBlock() override {return true;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
};

//////////////////////////////////////////////////////////////////////
//
// Symbol
//
//////////////////////////////////////////////////////////////////////

class MslSymbol : public MslNode
{
  public:
    MslSymbol(MslToken& t) : MslNode(t) {}
    virtual ~MslSymbol() {}

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
    
    // runtime state
    class Symbol* symbol = nullptr;
    class MslProc* proc = nullptr;
    class MslVar* var = nullptr;
    
    bool isSymbol() override {return true;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}


};

//////////////////////////////////////////////////////////////////////
//
// Operator
//
//////////////////////////////////////////////////////////////////////

class MslOperator : public MslNode
{
  public:
    MslOperator(MslToken& t) : MslNode(t) {}
    virtual ~MslOperator() {}

    // operators stop accepting nodes with all of their operands
    // are satisified
    // todo: need to support unary
    // disallow structural nodes like proc and var
    bool wantsNode(MslNode* node) override {
        bool wants = false;
        if (children.size() < 2 &&
            (node->operandable() ||
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
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

};

//////////////////////////////////////////////////////////////////////
//
// Assignment
//
//////////////////////////////////////////////////////////////////////

/**
 * Assignments are basically operators with added runtime semantics
 */
class MslAssignment : public MslNode
{
  public:
    MslAssignment(MslToken& t) : MslNode(t) {}
    virtual ~MslAssignment() {}

    bool wantsNode(MslNode* node) override {
        bool wants = false;
        // geez, just have an isAssignable or something
        if (children.size() < 2 &&
            (node->operandable() || node->isAssignment()))
          wants = true;
        return wants;
    }

    bool isAssignment() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

};    
    
//////////////////////////////////////////////////////////////////////
//
// Var
//
//////////////////////////////////////////////////////////////////////

class MslVar : public MslNode
{
  public:
    MslVar(MslToken& t) : MslNode(t) {}
    virtual ~MslVar() {}

    bool wantsToken(class MslParser* p, MslToken& t) override;

    // vars accept an expression
    bool wantsNode(MslNode* node) override {
        // okay this is the same as Operator and Assignment
        // except we only accept one child
        // need an isExpression() that encapsulates this
        bool wants = false;
        if (children.size() < 1 && node->operandable())
          wants = true;
        return wants;
    }

    juce::String name;

    bool isVar() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
};

//////////////////////////////////////////////////////////////////////
//
// Proc
//
//////////////////////////////////////////////////////////////////////

class MslProc : public MslNode
{
  public:
    MslProc(MslToken& t) : MslNode(t) {}
    virtual ~MslProc() {}

    // same as var
    bool wantsToken(class MslParser* p, MslToken& t) override;

    bool wantsNode(MslNode* node) override {
        bool wants = false;
        if (!hasArgs && node->isBlock() && node->token.value == "(") {
            hasArgs = true;
            wants = true;
        }
        else if (!hasBody && node->isBlock() && node->token.value == "{") {
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
    MslBlock* getBody() {
        MslBlock* body = nullptr;
        for (auto child : children) {
            if (child->isBlock() && child->token.value == "{") {
                body = static_cast<MslBlock*>(child);
                break;
            }
        }
        return body;
    }
    MslBlock* getDeclaration() {
        MslBlock* decl = nullptr;
        for (auto child : children) {
            if (child->isBlock() && child->token.value == "(") {
                decl = static_cast<MslBlock*>(child);
                break;
            }
        }
        return decl;
    }
};

//////////////////////////////////////////////////////////////////////
//
// If/Else
//
//////////////////////////////////////////////////////////////////////

class MslIf : public MslNode
{
  public:
    MslIf(MslToken& t) : MslNode(t) {}
    ~MslIf() {}

    bool isIf() override {return true;}

    // this one can get kind of weird with else
    // MslIf is the only thing that can receive an else so if
    // we find one dangling need to error,
    // rather than asking a target node if it wants a new node,
    // ask the new node if it wants to be inside the target?
    // is this any different, still have to move up the stack
    bool wantsNode(MslNode* node) override {
        bool wants = false;
        if (node->isElse()) {
            // only makes sense if we'e already got a condiition
            // and a truth block, otherwise it's a syntax error?
            wants = (children.size() == 2);
        }
        else {
            wants = (children.size() < 2);
        }
        return wants;
    }
    
    // old model just had a chain of conditionals and clauses
    // which might be better than embedding another MslIf inside the false block

    MslNode* condition = nullptr;
    MslNode* trueBlock = nullptr;;
    MslNode* falseBlock = nullptr;
    
    void visit(MslVisitor* v) override {v->mslVisit(this);}
};

class MslElse : public MslNode
{
  public:
    MslElse(MslToken& t) : MslNode(t) {}
    ~MslElse() {}

    bool isElse() override {return true;}

    bool wantsNode(MslNode* node) override {
        (void)node;
        return (children.size() == 0);
    }
    
    void visit(MslVisitor* v) override {v->mslVisit(this);}
};

//////////////////////////////////////////////////////////////////////
//
// Flow Control
//
// End, Break, Return, Jump, Label
//
//////////////////////////////////////////////////////////////////////

class MslEnd : public MslNode
{
  public:
    MslEnd(MslToken& t) : MslNode(t) {}
    ~MslEnd() {}

    bool isEnd() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    bool operandable() {return false;}
};

class MslEcho : public MslNode
{
  public:
    MslEcho(MslToken& t) : MslNode(t) {}
    ~MslEcho() {}

    bool wantsNode(MslNode* node) override {
        (void)node;
        return (children.size() == 0);
    }
    
    bool isEcho() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    bool operandable() {return false;}
};

//////////////////////////////////////////////////////////////////////
//
// Repetition
//
// Repeat, While, Until
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
// Scope
//
// In, For?
//
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
// Threads
//
// Launch, Suspend, Resume
//
//////////////////////////////////////////////////////////////////////

/**
 * context <name>
 *
 * Switches the thread context running this script.
 * There are currently two contexts: "shell" and "kernel"
 * Most of the time contexts will be switched automatically but this
 * can be used to force it into a context for testing or to preemptively
 * put the script in a context that will eventually be required and avoid
 * a transition delay.
 *
 * Alternate names for shell are: ui
 * Alternate names for kernel are: audio
 */
class MslContextNode : public MslNode
{
  public:
    MslContextNode(MslToken& t) : MslNode(t) {}
    ~MslContextNode() {}

    bool wantsToken(class MslParser* p, MslToken& t) override;

    bool wantsNode(MslNode* node) override {
        (void)node;
        return false;
    }

    bool isContext() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    bool operandable() {return false;}

    // the default is kernel since where most things happen
    bool shell = false;
    bool finished = false;
};

//////////////////////////////////////////////////////////////////////
//
// Wait
//
//////////////////////////////////////////////////////////////////////

/**
 * Implementation of this one is more complex and broken out into MslWaitNode.cpp
 * The class has the Node suffix so it doesn't conflict with MslWait which needs
 * to be public.  
 */
class MslWaitNode : public MslNode
{
  public:
    MslWaitNode(MslToken& t) : MslNode(t) {}
    ~MslWaitNode() {}

    bool isWait() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    bool operandable() override {return false;}
    
    bool wantsToken(class MslParser* p, MslToken& t) override;
    bool wantsNode(MslNode* node) override;

    MslWaitType type = WaitTypeNone;
    MslWaitEvent event = WaitEventNone;
    MslWaitDuration duration = WaitDurationNone;
    MslWaitLocation location = WaitLocationNone;
    juce::String typeName;
    bool error = false;

    // public for the console
    const char* typeToKeyword(MslWaitType e);
    const char* eventToKeyword(MslWaitEvent e);
    const char* durationToKeyword(MslWaitDuration e);
    const char* locationToKeyword(MslWaitLocation e);

  private:
    const char* enumToKeyword(const char* keywords[], int e);
    int keywordToEnum(const char* keywords[], const char* key);
    MslWaitType keywordToType(const char* s);
    MslWaitEvent keywordToEvent(const char* s);
    MslWaitDuration keywordToDuration(const char* s);
    MslWaitLocation keywordToLocation(const char* s);
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

