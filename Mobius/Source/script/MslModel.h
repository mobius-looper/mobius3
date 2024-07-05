/**
 * Parse tree model for MSL scripts
 */

#pragma once

#include <JuceHeader.h>

class MslVisitor
{
  public:
    virtual ~MslVisitor() {}

    virtual void mslVisit(class MslLiteral* obj) = 0;
    virtual void mslVisit(class MslSymbol* obj) = 0;
    virtual void mslVisit(class MslBlock* obj) = 0;
    virtual void mslVisit(class MslOperator* obj) = 0;
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

    virtual bool isOpen(MslNode* node) {(void)node; return false;}

    virtual bool isComplete() {
        bool complete = true;
        for (auto child : children) {
            if (child->isComplete()) {
                complete = false;
                break;
            }
        }
        return complete;
    }

    virtual bool isLiteral() {return false;}
    virtual bool isSymbol() {return false;}
    virtual bool isBlock() {return false;}
    virtual bool isOperator() {return false;}

    virtual void visit(MslVisitor* visitor) = 0;
};

class MslLiteral : public MslNode
{
  public:
    MslLiteral(juce::String s) : MslNode(s) {}
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

class MslSymbol : public MslNode
{
  public:
    MslSymbol(juce::String s) : MslNode(s) {}
    virtual ~MslSymbol() {}

    bool isSymbol() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

    class Symbol* symbol = nullptr;

    // A symbol allows an arguments and body blocks
    // but only one of each
    bool isOpen(MslNode* node) override {
        bool open = false;
        if (node == nullptr) {
            // it's asking if we might accept something, sure
            open = true;
        }
        else {
            // a block came in
            // allow either, but may only proc symbols should allow bodies
            if (node->token == "(" || node->token == "{")
              open = !hasBlock(node->token);
        }
        return open;
    }

};

class MslBlock : public MslNode
{
  public:
    MslBlock(juce::String token) : MslNode(token) {}
    virtual ~MslBlock() {}

    bool isBlock() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}
    bool isOpen(MslNode* node) override {(void)node; return true;}
};

class MslOperator : public MslNode
{
  public:
    MslOperator(juce::String s) : MslNode(s) {}
    virtual ~MslOperator() {}

    bool isOperator() override {return true;}
    void visit(MslVisitor* v) override {v->mslVisit(this);}

    bool isOpen(MslNode* node) override {
        (void)node;
        // todo: handle uniaryness here or in the parser?
        // todo: handle precedence here or in the parser?
        return (children.size() < 2);
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

