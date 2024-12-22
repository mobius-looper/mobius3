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

    virtual void mslVisit(class MslLiteralNode* obj) = 0;
    virtual void mslVisit(class MslSymbolNode* obj) = 0;
    virtual void mslVisit(class MslBlockNode* obj) = 0;
    virtual void mslVisit(class MslOperatorNode* obj) = 0;
    virtual void mslVisit(class MslAssignmentNode* obj) = 0;
    virtual void mslVisit(class MslVariableNode* obj) = 0;
    virtual void mslVisit(class MslFunctionNode* obj) = 0;
    virtual void mslVisit(class MslIfNode* obj) = 0;
    virtual void mslVisit(class MslElseNode* obj) = 0;
    virtual void mslVisit(class MslReferenceNode* obj) = 0;
    virtual void mslVisit(class MslEndNode* obj) = 0;
    virtual void mslVisit(class MslWaitNode* obj) = 0;
    virtual void mslVisit(class MslPrintNode* obj) = 0;
    virtual void mslVisit(class MslContextNode* obj) = 0;
    virtual void mslVisit(class MslInNode* obj) = 0;
    virtual void mslVisit(class MslSequenceNode* obj) = 0;
    virtual void mslVisit(class MslArgumentNode* obj) = 0;
    virtual void mslVisit(class MslKeywordNode* obj) = 0;
    virtual void mslVisit(class MslTraceNode* obj) = 0;
    virtual void mslVisit(class MslPropertyNode* obj) = 0;
    virtual void mslVisit(class MslCaseNode* obj) = 0;
    virtual void mslVisit(class MslResultNode* obj) = 0;
    // this one doesn't need a visitor
    virtual void mslVisit(class MslInitNode* obj) {
        (void)obj;
    }
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

    void clear() {
        children.clear();
    }
    
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

    // returns true if the node is finished and can be removed from the stack
    virtual bool isFinished() {
        return false;
    }
    
    // returns true if the node would like to consume the token
    virtual bool wantsToken(class MslParser* p, MslToken& t) {
        (void)p; (void)t; return false;
    }
    
    virtual class MslPropertyNode* wantsProperty(class MslParser* p, MslToken& t) {
        (void)p; (void)t; return nullptr;
    }

    // returns true if the node would like to consume another child node
    // do we really need locked if this works?
    virtual bool wantsNode(class MslParser* p, MslNode* n) {
        (void)p; (void)n; return false;
    }

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

    virtual const char* getLogName() {return "?";}

    virtual class MslLiteralNode* getLiteral() {return nullptr;}
    virtual class MslSymbolNode* getSymbol() {return nullptr;}
    virtual class MslBlockNode* getBlock() {return nullptr;}
    virtual class MslOperatorNode* getOperator() {return nullptr;}
    virtual class MslAssignmentNode* getAssignment() {return nullptr;}
    virtual class MslVariableNode* getVariable() {return nullptr;}
    virtual class MslFunctionNode* getFunction() {return nullptr;}
    virtual class MslScopedNode* getScopedNode() {return nullptr;}
    virtual class MslIfNode* getIf() {return nullptr;}
    virtual class MslElseNode* getElse() {return nullptr;}
    virtual class MslCaseNode* getCase() {return nullptr;}
    virtual class MslReferenceNode* getReference() {return nullptr;}
    virtual class MslEndNode* getEnd() {return false;}
    virtual class MslWaitNode* getWait() {return nullptr;}
    virtual class MslPrintNode* getPrint() {return nullptr;}
    virtual class MslContextNode* getContext() {return nullptr;}
    virtual class MslInNode* getIn() {return nullptr;}
    virtual class MslSequenceNode* getSequence() {return nullptr;}
    virtual class MslArgumentNode* getArgument() {return nullptr;}
    virtual class MslKeywordNode* getKeyword() {return nullptr;}
    virtual class MslInitNode* getInit() {return nullptr;}
    virtual class MslTraceNode* getTrace() {return nullptr;}
    virtual class MslPropertyNode* getProperty() {return nullptr;}
    virtual class MslResultNode* getResult() {return nullptr;}
    
    bool isLiteral() {return getLiteral() != nullptr;}
    bool isSymbol() {return getSymbol() != nullptr;}
    bool isBlock() {return getBlock() != nullptr;}
    bool isOperator() {return getOperator() != nullptr;}
    bool isAssignment() {return getAssignment() != nullptr;}
    bool isVariable() {return getVariable() != nullptr;}
    bool isFunction() {return getFunction() != nullptr;}
    bool isScoped() {return getScopedNode() != nullptr;}
    bool isIf() {return getIf() != nullptr;}
    bool isElse() {return getElse() != nullptr;}
    bool isCase() {return getCase() != nullptr;}
    bool isReference() {return getReference() != nullptr;}
    bool isEnd() {return getEnd() != false;}
    bool isWait() {return getWait() != nullptr;}
    bool isPrint() {return getPrint() != nullptr;}
    bool isContext() {return getContext() != nullptr;}
    bool isIn() {return getIn() != nullptr;}
    bool isSequence() {return getSequence() != nullptr;}
    bool isArgument() {return getArgument() != nullptr;}
    bool isKeyword() {return getKeyword() != nullptr;}
    bool isInit() {return getInit() != nullptr;}
    bool isTrace() {return getTrace() != nullptr;}
    bool isProperty() {return getProperty() != nullptr;}
    bool isResult() {return getResult() != nullptr;}
    
    virtual void link(class MslContext* context, class MslEnvironment* env, class MslResolutionContext* rc, class MslCompilation* comp) {
        (void)context;
        (void)env;
        (void)rc;
        (void)comp;
    }
    
    virtual void visit(MslVisitor* visitor) = 0;

    // console tools
    // what was this for?
#if 0
    void detach() {
        if (parent != nullptr) {
            parent->remove(this);
        }
    }
#endif

    /**
     * Return the name of the node token as a C string
     * This is used all the time for trace messages.
     */
    const char* getName() {
        return token.value.toUTF8();
    }
    
};

//////////////////////////////////////////////////////////////////////
//
// Literal
//
//////////////////////////////////////////////////////////////////////

class MslLiteralNode : public MslNode
{
  public:
    MslLiteralNode(MslToken& t) : MslNode(t) {locked=true;}
    virtual ~MslLiteralNode() {}

    MslLiteralNode* getLiteral() override {return this;}
    bool isFinished() override {return true;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Literal";}
    
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
// Keyword
//
// Keywords have the form :name
// Will eventually evolve into Qualified names of the form scope:name
//
// hmm, now that we have this, this could be merged with MslReferenceNode?
//   $1 vs :1
//
// except that references always return something and :foo doesn't
//
//////////////////////////////////////////////////////////////////////

class MslKeywordNode : public MslNode
{
  public:
    MslKeywordNode(MslToken& t) : MslNode(t) {locked=true;}
    virtual ~MslKeywordNode() {}

    MslKeywordNode* getKeyword() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Keyword";}

    // if you want to use these as another form of string quoting
    // e.g. if muteMode == :cancel  then they have to be operands
    bool operandable() override {return true;}

    // take the next symbol
    bool wantsToken(class MslParser* p, MslToken& t) override;
    
    juce::String name;

};

//////////////////////////////////////////////////////////////////////
//
// Reference
//
//////////////////////////////////////////////////////////////////////

class MslReferenceNode : public MslNode
{
  public:
    MslReferenceNode(MslToken& t) : MslNode(t) {locked=true;}
    virtual ~MslReferenceNode() {}

    // take the next number or symbol
    // if it isn't one of those raise an error
    bool wantsToken(class MslParser* p, MslToken& t) override;
    
    juce::String name;
    
    MslReferenceNode* getReference() override {return this;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Reference";}
};

//////////////////////////////////////////////////////////////////////
//
// Block
//
//////////////////////////////////////////////////////////////////////

class MslBlockNode : public MslNode
{
  public:
    MslBlockNode(MslToken& t) : MslNode(t) {}
    // special constructor for the root block with no tokcn
    MslBlockNode() {}
    virtual ~MslBlockNode() {}

    // doesn't want tokens but will always accept nodes
    // might want tokens if inner blocks allow declarations
    // or are those handled as preprocessor lines?
    // I guess this is where locking comes in to play
    // unless we consume the close bracket token and remember that
    // to make wantsNode return false, this will always hapily take nodes
    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node; return true;
    }

    // blocks accumulate procs and vars defined within it outside of the
    // logic node tree

    juce::OwnedArray<MslFunctionNode> functions;
    juce::OwnedArray<MslVariableNode> variables;

    MslBlockNode* getBlock() override {return this;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Block";}
};

/**
 * A sequence is similar to a block except that it is not enclosed in brackets
 * It accumulates nodes as long as a ',' token is received
 * One of these is not created automatically during parsing, it needs to be
 * injected where necessary, notably after an "in"
 */
class MslSequenceNode : public MslNode
{
  public:
    // these are not created from tokens
    MslSequenceNode() {}
    virtual ~MslSequenceNode() {}

    // this one is a little weird, it wants to SEE the token
    // but it doesn't consume it so , continues to act as a terminator
    // for theh last node
    bool wantsToken(class MslParser* p, MslToken& t) override {
        (void)p;
        if (t.value == ",")
          armed = true;
        return false;
    }

    // should be more restritive here, procs and vars for example
    // don't make sense in a sequence, they are not binding blocks
    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node;
        bool wants (children.size() == 0 || armed);
        armed = false;
        return wants;
    }

    bool armed = false;

    // should this be a block subclass?
    MslSequenceNode* getSequence() override {return this;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Sequence";}
};


//////////////////////////////////////////////////////////////////////
//
// Operator
//
//////////////////////////////////////////////////////////////////////

/**
 * Enumeration of operator codes, ranked in increasing order of precedence.
 * Try to be consistent with C++ precendece with many of the operators left out.
 * The p in pemdas is already handled outside operator parsing, and we don't
 * support exponents, so it's mdas plus the logical operators.
 *
 * Single = is the assignment operator not the equality operator.
 */
typedef enum {
    
    MslUnknown,

    MslEq,  // actually assignment
    MslOr,
    MslAnd,
    
    MslDeq,  // Eq/Neq are the same, supposed to be left to right
    MslNeq,

    MslGte,
    MslGt,
    MslLte,
    MslLt,
    
    MslMinus,
    MslPlus,
    MslDiv,
    MslMult,

    MslNot
    
} MslOperators;

class MslOperatorNode : public MslNode
{
  public:
    MslOperatorNode(MslToken& t) : MslNode(t) {}
    virtual ~MslOperatorNode() {}

    // operators stop accepting nodes with all of their operands
    // are satisified
    // todo: need to support unary
    // disallow structural nodes like proc and var
    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p;
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

    MslOperators opcode;

    static MslOperators mapOperator(juce::String& s);
    static MslOperators mapOperatorSymbol(juce::String& s);
    
    // runtime
    MslOperatorNode* getOperator() override {return this;}
    bool operandable() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Operator";}

};

//////////////////////////////////////////////////////////////////////
//
// Assignment
//
//////////////////////////////////////////////////////////////////////

/**
 * Assignments are basically operators with added runtime semantics
 */
class MslAssignmentNode : public MslNode
{
  public:
    MslAssignmentNode(MslToken& t) : MslNode(t) {}
    virtual ~MslAssignmentNode() {}

    bool wantsNode(class MslParser*p, MslNode* node) override {
        (void)p;
        bool wants = false;
        // geez, just have an isAssignable or something
        if (children.size() < 2 &&
            (node->operandable() || node->isAssignment()))
          wants = true;
        return wants;
    }

    MslAssignmentNode* getAssignment() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Assignment";}

};    
    
//////////////////////////////////////////////////////////////////////
//
// Variable
//
//////////////////////////////////////////////////////////////////////

class MslScopedNode : public MslNode
{
  public:
    // special no-arg constructor for the temporary keyword holder during parsing
    MslScopedNode() {}
    MslScopedNode(MslToken& t) : MslNode(t) {}
    virtual ~MslScopedNode() {}
    MslScopedNode* getScopedNode() override {return this;}
    void visit(MslVisitor* visitor) override {(void)visitor;}

    // hating this, look at how MslPropertyNode evolved
    bool keywordPublic = false;
    bool keywordExport = false;
    bool keywordGlobal = false;
    bool keywordScope = false;
    bool keywordPersistent = false;

    bool wantsToken(class MslParser* p, MslToken& t) override;
    bool hasScope();
    bool isStatic();
    void resetScope();
    void transferScope(MslScopedNode* dest);
};

class MslPropertyNode : public MslNode
{
  public:
    MslPropertyNode(MslToken& t) : MslNode(t) {}

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node;
        return (children.size() < 1);
    }

    MslPropertyNode* getProperty() override {return this;}
    bool isFinished() override {return children.size() > 0;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Property";}
};

class MslVariableNode : public MslScopedNode
{
  public:
    MslVariableNode(MslToken& t) : MslScopedNode(t) {}
    virtual ~MslVariableNode() {}

    bool wantsToken(class MslParser* p, MslToken& t) override;
    MslPropertyNode* wantsProperty(class MslParser* p, MslToken& t) override;

    // vars accept an expression
    bool wantsNode(class MslParser* p, MslNode* node) override;

    juce::String name;
    bool wantsInitializer = false;
    class MslVariable* staticVariable = nullptr;
    juce::OwnedArray<MslPropertyNode> properties;
    
    MslVariableNode* getVariable() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Variable";}
};

//////////////////////////////////////////////////////////////////////
//
// Function
//
//////////////////////////////////////////////////////////////////////

class MslFunctionNode : public MslScopedNode
{
  public:
    MslFunctionNode(MslToken& t) : MslScopedNode(t) {}
    virtual ~MslFunctionNode() {}

    // same as var
    bool wantsToken(class MslParser* p, MslToken& t) override;

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p;
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
    
    MslFunctionNode* getFunction() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Function";}
    
    MslBlockNode* getBody() {
        MslBlockNode* body = nullptr;
        for (auto child : children) {
            MslBlockNode* b = child->getBlock();
            if (b != nullptr && child->token.value == "{") {
                body = b;
                break;
            }
        }
        return body;
    }
    MslBlockNode* getDeclaration() {
        MslBlockNode* decl = nullptr;
        for (auto child : children) {
            MslBlockNode* b = child->getBlock();
            if (b != nullptr  && child->token.value == "(") {
                decl = b;
                break;
            }
        }
        return decl;
    }
};

/**
 * An init is basicall a nameless function
 * Reconsider how this is modeled, a "named block" might
 * be more generally useful.
 */
class MslInitNode : public MslNode
{
  public:
    MslInitNode(MslToken& t) : MslNode(t) {}
    virtual ~MslInitNode() {}

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node;
        return (children.size() != 1);
    }
    
    MslInitNode* getInit() override {return this;}
    void visit(MslVisitor* v) override {(void)v;}
    const char* getLogName() override {return "Init";}
};

//////////////////////////////////////////////////////////////////////
//
// If/Else
//
//////////////////////////////////////////////////////////////////////

class MslIfNode : public MslNode
{
  public:
    MslIfNode(MslToken& t) : MslNode(t) {}
    ~MslIfNode() {}

    MslIfNode* getIf() override {return this;}

    // this one can get kind of weird with else
    // MslIfNode is the only thing that can receive an else so if
    // we find one dangling need to error,
    // rather than asking a target node if it wants a new node,
    // ask the new node if it wants to be inside the target?
    // is this any different, still have to move up the stack
    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p;
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
    // which might be better than embedding another MslIfNode inside the false block

    MslNode* condition = nullptr;
    MslNode* trueBlock = nullptr;;
    MslNode* falseBlock = nullptr;
    
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "If";}
};

class MslElseNode : public MslNode
{
  public:
    MslElseNode(MslToken& t) : MslNode(t) {}
    ~MslElseNode() {}

    MslElseNode* getElse() override {return this;}

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node;
        return (children.size() == 0);
    }
    
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Else";}
};

class MslCaseNode : public MslNode
{
  public:
    MslCaseNode(MslToken& t) : MslNode(t) {}
    ~MslCaseNode() {}

    MslCaseNode* getCase() override {return this;}

    bool wantsNode(class MslParser* p, MslNode* node) override;
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Case";}
};

//////////////////////////////////////////////////////////////////////
//
// Flow Control
//
// End, Break, Return, Jump, Label
//
//////////////////////////////////////////////////////////////////////

class MslEndNode : public MslNode
{
  public:
    MslEndNode(MslToken& t) : MslNode(t) {}
    ~MslEndNode() {}

    MslEndNode* getEnd() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "End";}
    bool operandable() override {return false;}
};

class MslPrintNode : public MslNode
{
  public:
    MslPrintNode(MslToken& t) : MslNode(t) {}
    ~MslPrintNode() {}

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node;
        return (children.size() == 0);
    }
    
    MslPrintNode* getPrint() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Print";}
    bool operandable() override {return false;}
};

class MslTraceNode : public MslNode
{
  public:
    MslTraceNode(MslToken& t) : MslNode(t) {}
    ~MslTraceNode() {}

    bool control = false;
    bool on = false;

    bool wantsToken(class MslParser* p, MslToken& t) override;

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node;
        return (!control && children.size() == 0);
    }
    
    MslTraceNode* getTrace() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Trace";}
    bool operandable() override {return false;}
};

class MslResultNode : public MslNode
{
  public:
    MslResultNode(MslToken& t) : MslNode(t) {}
    ~MslResultNode() {}

    MslResultNode* getResult() override {return this;}

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node;
        return (children.size() == 0);
    }
    
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Result";}
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
//////////////////////////////////////////////////////////////////////

class MslInNode : public MslNode
{
  public:
    MslInNode(MslToken& t) : MslNode(t) {}
    virtual ~MslInNode() {}

    bool hasArgs = false;
    bool hasBody = false;
    
    MslInNode* getIn() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "In";}

    bool wantsNode(class MslParser* p, MslNode* n) override {
        (void)p; (void)n;
        return (children.size() < 2);
    }
};

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

    bool wantsNode(class MslParser* p, MslNode* node) override {
        (void)p; (void)node;
        return false;
    }

    MslContextNode* getContext() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Context";}
    bool operandable() override {return false;}

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

    // parsing
    
    bool operandable() override {return false;}
    bool wantsToken(class MslParser* p, MslToken& t) override;
    bool wantsNode(class MslParser* p, MslNode* node) override;
    bool waitingForAmount = false;
    bool waitingForNumber = false;
    bool waitingForRepeat = false;
    
    // runtime

    MslWaitNode* getWait() override {return this;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    const char* getLogName() override {return "Wait";}
    
    MslWaitType type = MslWaitNone;
    bool next = false;
    int amountNodeIndex = -1;
    int numberNodeIndex = -1;
    int repeatNodeIndex = -1;
    
  private:
    
    bool isWaitingForNumber();

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

