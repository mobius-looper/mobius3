/**
 * Implementations for MslNode methods that need to have their
 * definitions hidden to avoid adding gratuituous dependencies on
 * everything that includes MslModel.h.  Mostly this is MslParser.h
 * used to inject error messages during token parsing.
 */

#include "MslModel.h"
#include "MslParser.h"

bool MslTrace::wantsToken(MslParser* p, MslToken& t)
{
    (void)p;
    bool wants = false;
    if (children.size() == 0) {
        if (t.type == MslToken::Type::Symbol) {
            if (t.value == "on") {
                control = true;
                on = true;
                wants = true;
            }
            else if (t.value == "off") {
                control = true;
                on = false;
                wants = true;
            }
        }
    }
    return wants;
}

bool MslReference::wantsToken(MslParser* p, MslToken& t)
{
    bool wants = false;
    if (name.length() == 0) {
        if (t.type == MslToken::Type::Symbol ||
            t.type == MslToken::Type::Int) {
            name = t.value;
            wants = true;
        }
        else {
            p->errorSyntax(t, "Invalid reference");
        }
    }
    return wants;
}

bool MslKeyword::wantsToken(MslParser* p, MslToken& t)
{
    bool wants = false;
    if (name.length() == 0) {
        if (t.type == MslToken::Type::Symbol) {
            name = t.value;
            wants = true;
        }
        else {
            p->errorSyntax(t, "Invalid keyword");
            // todo: could also check this against the set of known keywords
        }
    }
    return wants;
}

bool MslScopedNode::wantsToken(MslParser* p, MslToken& t)
{
    (void)p;
    bool wants = false;

    if (t.value == "public") {
        keywordPublic = true;
        wants = true;
    }
    else if (t.value == "export") {
        keywordExport = true;
        wants = true;
    }
    else if (t.value == "global" || t.value == "static") {
        keywordGlobal = true;
        wants = true;
    }
    else if (t.value == "track" || t.value == "scope") {
        keywordScope = true;
        wants = true;
    }
    return wants;
}

bool MslScopedNode::hasScope()
{
    return (keywordPublic || keywordExport || keywordGlobal || keywordScope);
}

void MslScopedNode::transferScope(MslScopedNode* dest)
{
    dest->keywordPublic = keywordPublic;
    dest->keywordExport = keywordExport;
    dest->keywordGlobal = keywordGlobal;
    dest->keywordScope = keywordScope;
    resetScope();
}

void MslScopedNode::resetScope()
{
    keywordPublic = false;
    keywordExport = false;
    keywordGlobal = false;
    keywordScope = false;
}

/**
 * var is one of the few that consumes tokens
 * hmm, it's a little more than this, it REQUIRES a token
 * wantsToken doesn't have a way to reject with prejeduce
 * we'll end up with a bad parse tree that will have to be caught at runtime
 * update: added error returns in MslParser
 */
bool MslVariableNode::wantsToken(MslParser* p, MslToken& t)
{
    (void)p;
    
    bool wants = MslScopedNode::wantsToken(p, t);
    if (!wants) {
        if (name.length() == 0) {
            if (t.type == MslToken::Type::Symbol) {
                // take this as our name
                name =  t.value;
                wants = true;
            }
        }
        else if (t.type == MslToken::Type::Operator &&
                 t.value == "=") {
            // skip past this once we have a name
            wants = true;
        }
        else {
            // now that we can stick errors in MslParser, is this
            // where that should go?
        }
    }
    return wants;
}

bool MslFunctionNode::wantsToken(MslParser* p, MslToken& t)
{
    (void)p;
    
    bool wants = MslScopedNode::wantsToken(p, t);
    if (!wants) {
        if (name.length() == 0) {
            if (t.type == MslToken::Type::Symbol) {
                name =  t.value;
                wants = true;
            }
        }
    }
    return wants;
}

bool MslContextNode::wantsToken(MslParser* p, MslToken& t)
{
    bool wants = false;
    if (!finished) {
        if (t.type == MslToken::Type::Symbol) {
            if (t.value == "shell" || t.value == "ui") {
                shell = true;
                finished = true;
            }
            else if (t.value == "kernel" || t.value == "audio") {
                shell = false;
                finished = true;
            }
        }
        if (finished)
          wants = true;
        else {
            p->errorSyntax(t, "Invalid context name");
        }
    }
    return wants;
}

//////////////////////////////////////////////////////////////////////
//
// Operators
//
//////////////////////////////////////////////////////////////////////

/**
 * Convert the operator token into an enumeration that is easier to deal
 * with after parsing.
 */

MslOperators MslOperator::mapOperator(juce::String& s)
{
    MslOperators op = MslUnknown;
    
    if (s == "+") op = MslPlus;
    else if (s == "-") op = MslMinus;
    else if (s == "*") op = MslMult;
    else if (s == "/") op = MslDiv;
    else if (s == "=") op = MslEq;
    else if (s == "==") op = MslDeq;
    else if (s == "!=") op = MslNeq;
    else if (s == ">") op = MslGt;
    else if (s == ">=") op = MslGte;
    else if (s == "<") op = MslLt;
    else if (s == "<=") op = MslLte;
    else if (s == "!") op = MslNot;
    else if (s == "&&") op = MslAnd;
    else if (s == "||") op = MslOr;

    // will they try to use this?
    //else if (s == "&") op = MslAmp;

    return op;
}

MslOperators MslOperator::mapOperatorSymbol(juce::String& s)
{
    MslOperators op = MslUnknown;
    
    if (s.equalsIgnoreCase("and")) op = MslAnd;
    else if (s.equalsIgnoreCase("or")) op = MslOr;
    else if (s.equalsIgnoreCase("not")) op = MslNot;
    else if (s.equalsIgnoreCase("eq")) op = MslDeq;
    else if (s.equalsIgnoreCase("neq")) op = MslNeq;
    else if (s.equalsIgnoreCase("equal")) op = MslDeq;
    else if (s.equalsIgnoreCase("equals")) op = MslDeq;

    return op;
}    
